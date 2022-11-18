# Amiga spi-lib

Original text by Niklas Ekstrom, modifications and additonal text by Dennis van Weeren
The functionality of the simple SPI controller is exposed through the spi-lib source code library.
Just include the three files in the project you are building.

Compilation has been tested to work with VBCC on an Amiga 3000.

The spi-lib exposes a small number of methods:

- spi_initialize(unsigned char channel); - initializes the library. Must be called before any other method. The simple SPI controller has 2 channels and the caller must specifiy which channel to use.
- spi_shutdown() - should be called when shutting down to reset the controller to its unused state.
- spi_speed(long speed) - the simple SPI controller can run in slow (<250 kHz) or fast (~600kHz) mode. A SPI peripheral may need to run in the slow mode during initialization. The speed is set to slow by default when spi-lib is initialized.
- spi_select() / spi_deselect() - activates/deactivates the SPI chip select pin.
- spi_read(char *buf, long size) - reads size bytes (1 <= size <= 65535) from the SPI peripheral and writes them to the buffer pointed to by buf.
- spi_write(char *buf, long size) - writes size bytes (1 <= size <= 65535) to the SPI peripheral that are taken from the buffer pointed to by buf.
- void spi_obtain() / void spi_release() - obtains/releases the SPI bus. The SPI bus is shared between devices/drivers and any driver must obtain the bus before doing anything! The bus should also be released when done so that other device drivers can use the bus.
