# a500-simple-spi-drivers
SD-card and Ethernet drivers for the Amiga 500 Simple SPI controller. 
The code is based upon previous work by ([Niklas Ekstrom](https://github.com/niklasekstrom/amiga-par-to-spi-adapter)) and ([Mike Sterling](https://github.com/mikestir/k1208-drivers)). Thanks to their excellent work I was able to build these drivers.

## How to compile and where to find the binaries
All code is compiled using VBCC hosted on an actual Amiga 3000.
Included are Amiga scripts (iconX) to call the compiler and build the code.
For compiling the network driver sana2.h is also needed. As this file is copyrighted I did not include it.
sana2.h should be placed in the sspi-net drawer
In the Amiga drawer you will find the Amiga binaries: 2 drivers and 2 test programs.
### sspisd.device
This is the SD-card driver, goes to devs: on your Amiga
### sspinet.device
This is the ENC28J60 network driver, also go to devs: on your Amiga
### sd_test
A simple console program to test the SD-card. It will read and dump sector 0 on the console.
### nic_test
A simple console program to test the enc28j60 ethernet controller.
It will reply every packet send and also dump it on the console.

# SD-card
The driver is currently very simple and does not support SCSI inquiry commands (so no HDtoolbox), no auto-mounting and no auto-booting.
The way to deal with this is to prep the SD-card under Winuae and use a manual mount script. As the driver is based upon Niklas's driver you can refer to the tutorials written for the sdbox. Like for example ([here](https://www.kernelcrash.com/blog/cheap-hard-drive-for-the-amiga-500-with-sdbox/2020/09/26/)). Just replace any reference to the driver with "sspisd.device". I actually followed above tutorial to create a boot floppy to mount the SD-card and handover control to the Workbench: partition on the SD-card. This process only takes a couiple of seconds and after that the SD-card behaves like any other harddisk.

#network driver


