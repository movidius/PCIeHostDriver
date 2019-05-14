obj-m := mxlk.o
ccflags-y += -Wall -Wno-unused-function -Werror
mxlk-objs := mxlk_pci.o mxlk_char.o mxlk_main.o mxlk_core.o mxlk_capabilities.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf *.o *.o.* *~

install:
	sudo insmod mxlk.ko

help:
	@echo ""
	@echo "make all     -> builds driver"
	@echo "make clean   -> delete build artifacts"
	@echo "make install -> loads mxlk.ko"
	@echo ""

TEST_HW_PLATFORM := ”HOST”
