#include <xpcc/architecture/platform.hpp>
#include <xpcc/debug/logger.hpp>

#include <arm_debug.h>
#include <swd_shortcuts.hpp>
#include <arm_etm.hpp>

// ----------------------------------------------------------------------------
// Set the log level
#undef	XPCC_LOG_LEVEL
#define	XPCC_LOG_LEVEL xpcc::log::DEBUG

// Create an IODeviceWrapper around the Uart Peripheral we want to use
xpcc::IODeviceWrapper< Usart2, xpcc::IOBuffer::BlockIfFull > loggerDevice;

// Set all four logger streams to use the UART
xpcc::log::Logger xpcc::log::debug(loggerDevice);
xpcc::log::Logger xpcc::log::info(loggerDevice);
xpcc::log::Logger xpcc::log::warning(loggerDevice);
xpcc::log::Logger xpcc::log::error(loggerDevice);

// ----------------------------------------------------------------------------

// STM32F407
static constexpr uint32_t UniqueIdAddress = 0x1FFF7A10;
static constexpr uint32_t FlashSizeAddress = 0x1FFF7A22;

// STM3F072
//static constexpr uint32_t UniqueIdAddress = 0x1FFFF7AC;
//static constexpr uint32_t DeviceIdAddress = 0x40015800;
//static constexpr uint32_t FlashSizeAddress = 0x1FFFF7CC;

enum class TraceMode : uint32_t {
	Async = 0, ///< UART async serial mode
	Sync1 = DBGMCU_CR_TRACE_MODE_0, ///< serial data out using one pin
	Sync2 = DBGMCU_CR_TRACE_MODE_1, ///< serial data out using two pins
	Sync4 = DBGMCU_CR_TRACE_MODE_1 | DBGMCU_CR_TRACE_MODE_0, ///< using four pins
};



static inline void start_etm_on_stm32f407(ARMDebug &tt) {
	// thank you to Petteri Aimonen for providing a working configuration!

	// comments below refer to sections in:
	// * RM0090: the RM0090 Reference Manual from ST
	// * TRM: ARM Cortex-M4 Processor Technical Reference Manual (r0p1)
	// * ARM: ARMv7-M Architecture Reference Manual
	// * ETM: Embedded Trace Macrocell Architecture Specification

	XPCC_LOG_DEBUG << "trying to enable etm on target..." << xpcc::endl;

	// see RM0090 section "38.16.3 Debug MCU configuration register"
	STR(&DBGMCU->CR,
	    static_cast<uint32_t>(TraceMode::Async)      |
	    DBGMCU_CR_TRACE_IOEN  | // actually enable the trace io
	    DBGMCU_CR_DBG_STANDBY | // this supposedly allows debugging when
	    DBGMCU_CR_DBG_STOP    | // the MCU is in low power mode
	    DBGMCU_CR_DBG_SLEEP);   // Just to be safe I guess?

	// see ARM section "C1.6.5 Debug Exception and Monitor Control Register (DEMCR)"
	// global enable
	STR(&CoreDebug->DEMCR, LDR(&CoreDebug->DEMCR) | CoreDebug_DEMCR_TRCENA_Msk);

	// see ARM section "C1.10.1 The TPIU Programmers' Model"
	STR(&TPI->ACPR, 0); // asynchronous trace clock divider set to HCLK/(x+1) = HCLK
	// to select Synchronous Trace Port Mode, write SPRR = 0
	STR(&TPI->SPPR, 2); // 2 => Asynchronous Serial Wire Output (NRZ)

	// see TRM section "11.3.4 Formatter and Flush Control Register, TPIU_FFCR"
	STR(&TPI->FFCR, TPI_FFCR_EnFCont_Msk | TPI_FFCR_TrigIn_Msk);

	// see ARM section "C1.8.2 Register support for the DWT"
	STR(&DWT->CTRL,
	    4 << DWT_CTRL_NUMCOMP_Pos | // number of comparators
	    DWT_CTRL_EXCTRCENA_Msk    | // enable exception trace
	    DWT_CTRL_PCSAMPLENA_Msk   | // See Table C1-18 on page C1-35 for details.
	    2 << DWT_CTRL_SYNCTAP_Pos | // synchronization packet rate, Tap at CYCCNT bit 26
	    DWT_CTRL_CYCTAP_Msk       | // select Tap at CYCCNT bit 10
	    DWT_CTRL_CYCCNTENA_Msk);    // enabe cycle counter

	// see ARM section "C1.7.2 Register support for the ITM"
	STR(&ITM->LAR, 0xc5acce55); // magic value to activate ITM
	STR(&ITM->TCR,
	    1 << ITM_TCR_TraceBusID_Pos | // TraceBus ID
	    ITM_TCR_DWTENA_Msk          | // event packet emission from DWT -> TPIU
	    ITM_TCR_SYNCENA_Msk         | // synchronization packet transmission for synchronous TPIU
	    ITM_TCR_ITMENA_Msk);          // enable ITM
	STR(&ITM->TER, 0xffffffff); // enable all stimulus ports

	// see ETM section "3.5.61 Lock Access Register, ETMLAR, ETMv3.2 and later"
	STR(&ETM->LAR, 0xc5acce55); // magic value to activate ETM
	// see ETM section "3.4.4 ETM Programming bit and associated state"
	STR(&ETM->CR, LDR(&ETM->CR) | ETM_CR_PROGRAMMING_Msk);
	STR(&ETM->CR,
	    ETM_CR_STALL_PROCESSOR_Msk |
	    ETM_CR_BRANCH_OUTPUT_Msk   |
	    ETM_CR_PROGRAMMING_Msk     |
	    ETM_CR_PORT_SELECTION_Msk);
	STR(&ETM->TRACEIDR, 2); // TraceBus ID
	// the following tells the ETM to "exclude nothing"
	STR(&ETM->TECR1, ETM_TECR1_INC_EXC_COTRL_Msk); // => "trace always enabled"
	// see ETM section "3.5.13 FIFOFULL Region Register, ETMFFRR"
	// "exclude nothing" (i.e. no mem region) from setting the fifo full flag
	STR(&ETM->FFRR, ETM_FFRR_INC_EXC_COTRL_Msk); // => "Stalling always enabled"
	STR(&ETM->FFLR, 24); // fifo full level <= 24
	// deassert ETM Programming bit
	STR(&ETM->CR, LDR(&ETM->CR) & ~ETM_CR_PROGRAMMING_Msk);
}

int
main()
{
	Board::initialize();

	// initialize Uart2 for XPCC_LOG_
	GpioOutputA2::connect(Usart2::Tx);
	GpioInputA3::connect(Usart2::Rx, Gpio::InputType::PullUp);
	Usart2::initialize<Board::systemClock, 115200>(12);

	//ARMDebug target(ARMDebug::LOG_TRACE_MEM);
	ARMDebug target;
	if(not target.begin()) {
		XPCC_LOG_ERROR << "failed to connect to target" << xpcc::endl;
		XPCC_LOG_ERROR << "will just keep looping...." << xpcc::endl;
		while(true)
			;
	}

	XPCC_LOG_INFO << "Unique Device Id: 0x";
	uint32_t data[3];
	target.memLoad(UniqueIdAddress, data, 3);
	XPCC_LOG_INFO << xpcc::hex << data[0] << data[1] << data[2] << xpcc::endl;

	XPCC_LOG_INFO << "Flash Size: ";
	uint32_t flash_size = 0;
	target.memLoad(FlashSizeAddress, flash_size);
	XPCC_LOG_INFO << (flash_size >> 16) << "kBytes" << xpcc::endl;

	start_etm_on_stm32f407(target);

	while (1)
	{
	}

	return 0;
}
