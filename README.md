# a500-simple-spi-drivers
SD-card and Ethernet drivers for the Amiga 500 Simple SPI controller. 

## How to compile and where to find the binaries
All code is compiled using VBCC hosted on an actual Amiga 3000.
Included are Amiga scripts (iconX) to call the compiler and build the code.
For compiling the network driver sana2.h is also needed. As this file is copyrighted I did not include it.
sana2.h should be placed in the sspi-net drawer
In the Amiga drawer you will find the Amiga binaries: 2 drivers and 2 test programs.
###sspisd.device
This is the SD-card driver, goes to devs: on your Amiga
###sspinet.device
This is the ENC28J60 network driver, also go to devs: on your Amiga
###sd_test
A simple console program to test the SD-card. It will read and dump sector 0 on the console.
###nic_test
A simple console program to test the enc28j60 ethernet controller.
It will reply every packet send and also dump it on the console.




