DRIVERDIR = Driver
USERPROGRAMDIR = UserSpace

SUBDIRS = $(DRIVERDIR) $(USERPROGRAMDIR)

all:
	@echo ##########################
	@echo #  Default target : all  #
	@echo ##########################

	for i in $(SUBDIRS) ; do \
		( cd $$i ; make ) ; \
	done

driver:
	@cd $(DRIVERDIR) ; make

userprogram:
	@cd $(USERPROGRAMDIR) ; make

clean:
	for i in $(SUBDIRS) ; do \
		( cd $$i ; make clean) ; \
	done
