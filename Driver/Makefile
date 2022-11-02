obj-m += nice_nordicdriver.o
 
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

nordic: modules
nordicinstall: modules_install

modules modules_install help clean:
	make -C $(KERNELDIR)  M=$(shell pwd) $@
