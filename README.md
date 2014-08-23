# ADS129X Arduino library

This library adds support for the ADS129X series of analog front-ends from Texas Instruments to the Arduino environment. It is based on work by [conorrussomanno](https://github.com/conorrussomanno/ADS1299).

So far the library has only been tested with the ADS1298. In theory it should be compatible to all members of the ADS129X family.

## Modes of operation

The library support two modes of operation: interrupt-driven and polling. Both modes use the same API and your sketch still has to poll for available data even when in interrupt mode. However in interrupt mode the response time to changes on the *DRDY* line is much quicker and thus higher sample-rates are supported (tested with up 8 kSPS). Interrupt mode is the default, to switch to the old polling operation add this to your sketch as the first line:

```arduino
#define ADS129X_POLLING
```

When multiple devices share the SPI bus you will want to use polling mode as not to interfere with the SPI transactions of other devices.

## Example sketches

Two example sketches are included. One transfers the data to a PC via a serial connection, the other uses a nRF8001 BTLE chip to send it to a phone. Both were tested using a custom board including an Olimex nRF8001 breakout and a Teensy 3.1.

Libraries required by the sketches:
* [github.com/PaulStoffregen/Adafruit_nRF8001/](https://github.com/PaulStoffregen/Adafruit_nRF8001/tree/bb385aed176389d806016617a18d7e347074bc3c)
* [github.com/duff2013/LowPower_Teensy3/](https://github.com/duff2013/LowPower_Teensy3/tree/721e3bcab47cc7cdb03f0aee9e11d47611430aa9)

License
-------

See [LICENSE](LICENSE.md)
