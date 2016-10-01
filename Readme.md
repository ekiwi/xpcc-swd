# xpcc-swd

This is a port of [scanlime](https://twitter.com/scanlime)'s swd code for the esp8266 to xpcc.

## test setup

To test this code you need a stm32f3 discovery board which serves as `SWD` host.
Just go into `stm32f3discovery` and execute `scons program` to flash the test
firmware. UART output is on GpioA2 @ 115200 baud.


### connect swd target

As target board I used a stm32f4 discovery board, but the swd code should
- in general - work with any microcontroller that features a swd debug port.

For the stm32f4 discovery board you need to do the following:

* remove the two jumper close to `ST-Link`/`DISCOVERY` print to disconnect SWD
  from the on board ST-Link debug adapter
* connect groud: GDN(stm32f3) to GND(stm32f4)
* connect swd clock: PC6(stm32f3) to PA14(stm32f4)
* connect swd data: PC7(stm32f3) to PA13(stm32f4)

Now you should be able to reset the stm32f3 swd host and see the following
printed through the UART interface:

```
Found ARM processor debug port (IDCODE: 2BA01477)
```
