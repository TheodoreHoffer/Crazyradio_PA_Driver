# Crazyradio_PA_Driver
***NordicDriverDemo* is a demonstration of usb device module writing for linux, based on the Bitcraze Crazyradio PA device.**

These sources contain :
- The fully functional driver.
- A very basic user program to show how to communicate with the device through the driver.
- A firmware bin file for the Bitcraze Crazyradio PA.

This driver is totally free to use. You can develop your own user program using this driver.

***Installation:***

To build the driver and the user program  : 
$ sudo make

To build only the driver :
$ sudo make driver

to build only the user program :
$ sudo make userprogram

To install the Driver module:
$ sudo insmod nice_nordicdriver.ko

(I chose to not automate an install procedure to let you decide if yo want to load the driver or not.

***Uninstall:***

To remove the driver module from your system modules:
$ sudo rmmod nice_nordicdriver

To clean the builds:
$ sudo make clean

***Usage:***

The driver part is straigthforward to use. Once installed, you have nothing more to do.

The user program example use your Crazyradio PA device to send some configuration options to a distant bitcraze
receiver.
you can send at once one or more configuration commands.

nordic_configurator
		      [--radiochannel] "0..125" 
		      [--datarate] "0,1,2\"
		      [--radiopower] "0..3\"
		      [--radioard] "0x80..0xA0\"
		      [--radioarc] "0x00..0x0F\"
		      |--ackenable] "0 or >0\"
		      [--continuouscarrier] "0 or >0"
		      ------------------------------------"

Example : sudo ./nordic_configurator --radiochannel 123 --ackenable 1

For options explanations, read, in "Documentation" directory:
doc_crazyradio_usb_index [Bitcraze Wiki].pdf

