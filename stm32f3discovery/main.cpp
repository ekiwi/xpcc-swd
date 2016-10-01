#include <xpcc/architecture/platform.hpp>
#include <xpcc/debug/logger.hpp>

#include <arm_debug.h>

// ----------------------------------------------------------------------------
// Set the log level
#undef	XPCC_LOG_LEVEL
#define	XPCC_LOG_LEVEL xpcc::log::INFO

// Create an IODeviceWrapper around the Uart Peripheral we want to use
xpcc::IODeviceWrapper< Usart2, xpcc::IOBuffer::BlockIfFull > loggerDevice;

// Set all four logger streams to use the UART
xpcc::log::Logger xpcc::log::debug(loggerDevice);
xpcc::log::Logger xpcc::log::info(loggerDevice);
xpcc::log::Logger xpcc::log::warning(loggerDevice);
xpcc::log::Logger xpcc::log::error(loggerDevice);

// ----------------------------------------------------------------------------
int
main()
{
	Board::initialize();

	// initialize Uart2 for XPCC_LOG_
	GpioOutputA2::connect(Usart2::Tx);
	GpioInputA3::connect(Usart2::Rx, Gpio::InputType::PullUp);
	Usart2::initialize<Board::systemClock, 115200>(12);

	ARMDebug target(ARMDebug::LOG_TRACE_MEM);
	if(not target.begin()) {
		XPCC_LOG_ERROR << "failed to connect to target" << xpcc::endl;
		XPCC_LOG_ERROR << "will just keep looping...." << xpcc::endl;
		while(true)
			;
	}

	// STM32F407 specific reads
	XPCC_LOG_INFO << "Unique Device Id: 0x";
	uint32_t data[3];
	target.memLoad(0x1FFF7A10, data, 3);
	XPCC_LOG_INFO << xpcc::hex << data[0] << data[1] << data[2] << xpcc::endl;

	XPCC_LOG_INFO << "Flash Size: ";
	uint32_t flash_size = 0;
	target.memLoad(0x1FFF7A22, flash_size);
	XPCC_LOG_INFO << (flash_size >> 16) << "kBytes" << xpcc::endl;
	while (1)
	{
	}

	return 0;
}
