# a500-simple-spi-drivers

# NOTE: The SD-card driver has been superseded by [sd-lide.device](https://github.com/Mathesar/sd_lide.device/tree/a500-simple-spi).

SD-card and Ethernet drivers for the Amiga 500 Simple SPI controller. 
The code is based upon previous work by [Niklas Ekstrom](https://github.com/niklasekstrom/amiga-par-to-spi-adapter) and [Mike Sterling](https://github.com/mikestir/k1208-drivers). Without their excellent work I would have never started this project.

## How to compile and where to find the binaries
All code was compiled using VBCC hosted on an actual Amiga 3000.
Included are Amiga scripts to call the compiler and build the code.
For compiling the network driver sana2.h is also needed. As this file is copyrighted I did not include it.
sana2.h should be placed in the sspi-net drawer
In the Amiga drawer you will find the Amiga binaries: 2 drivers and 2 test programs.
### sspisd.device
This is the SD-card device driver, goes to devs: on your Amiga
### sspinet.device
This is the SANA-II ENC28J60 network device driver, also go to devs: on your Amiga
### sd_test
A simple console program to test the SD-card. It will read and dump sector 0 on the console.
### nic_test
A simple console program to test the enc28j60 ethernet controller.
It will reply every packet send and also dump it on the console.

# SD-card
The driver is currently very simple and does not support SCSI inquiry commands (so no HDtoolbox), no auto-mounting and no auto-booting.
The way to deal with this is to prep the SD-card under Winuae and use a manual mount script. As the driver is based upon Niklas's driver you can refer to the tutorials written for the sdbox. Like for example ([here](https://www.kernelcrash.com/blog/cheap-hard-drive-for-the-amiga-500-with-sdbox/2020/09/26/)). Just replace any reference to the driver with "sspisd.device". I actually use a boot floppy to mount the SD-card and handover control to the Workbench: partition on the SD-card. This process only takes a couple of seconds and after that the SD-card behaves like any other harddisk.

# network driver
The network driver uses channel 2 of the simple SPI controller to communicate with an ENC28J60 10Mbit ethernet controller chip. These chips are available as cheap modules from the usual sources and are  easily hooked up to the simple SPI controller using some dupont jump wires. Only 6 pins are used, VCC, GND, SCK, SO, SI and CS. The other pins on the module (WOL, RESET, CLKOUT and INT) are not used. The nework device driver is SANA-II compatible and can be used with any of the Amiga TCP/IP stacks.
I use the old but free AmiTCP 3.0b2 TCP/IP stack from [Aminet](https://aminet.net/package/comm/net/AmiTCP-bin-30b2). AmiTCP is not easy to setup but luckily Patrik Axelsson and David Eriksson made this excellent [installation guide](http://megaburken.net/~patrik/AmiTCP_Install/). Just make sure you replace any references to the SANA-II network device (they use "cnet.device") to "sspinet.device". Installing AmiTCP is probably best done under WinAUE like I did.

# performance SD-card 
(All tests done on an 68000 Amiga 500 with 1MB chip and 1.5MB slow).
* Booting into Classic-WB takes about 32 seconds.
* Loading up HippoPlayer takes about 14 seconds.
* Sysinfo reports a disk transfer speed of about 42 kbytes/sec.

Note that in general usage the relatively low performance of the simple SPI controller is not really noticed. Usually the CPU is the bottleneck. It is only when loading large chunks of data (like loading a large mod file in HippoPlayer) that you notice the speed difference.

# performance network driver
(All tests done on an 68000 Amiga 500 with 1MB chip and 1.5MB slow)
* Downloading from Aminet using ncftp works with a transfer rate up to 11 kbytes/sec
* Inifinity Music Player 3 reports a network speed af about 20 kbytes/sec
* A 1472 bytes ping to my router takes about 120ms.

# issues
Like mentioned before, the SD-card driver is very simple and missing a lot of features. It does seem very stable though and my system is working fine with it. I never had a crash caused by the driver.
The enc28j60 driver does have some issues it seems. It works stable enough to download files from Aminet using ncftp, visit some telnet BBS's with Ncomm and telser.device and you can listen to music using IMP3. However, it will crash when the system calls expunge() (and that only happens when you stop AmiTCP using "stopnet" and then do "avail flush" to free up memory). But no one ever does that right? Otherwise the driver is pretty stable.

