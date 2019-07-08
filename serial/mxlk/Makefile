obj-m := mxlk.o
COMMON_DIR := ../../common
ccflags-y += -Wall -Wno-unused-function -Werror -I$(PWD)/$(COMMON_DIR)
mxlk-objs := $(COMMON_DIR)/mx_boot.o \
			 $(COMMON_DIR)/mx_pci.o \
			 $(COMMON_DIR)/mx_reset.o \
			 $(COMMON_DIR)/mx_utils.o
mxlk-objs += mxlk_main.o \
			 mxlk_capabilities.o \
			 mxlk_char.o \
			 mxlk_core.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf *.o *.o.* *~
	rm -rf $(COMMON_DIR)/*.o $(COMMON_DIR)/.*.o.cmd $(COMMON_DIR)/.cache.mk

install:
	sudo insmod mxlk.ko

help:
	@echo ""
	@echo "make all     -> builds driver"
	@echo "make clean   -> delete build artifacts"
	@echo "make install -> loads mxlk.ko"
	@echo ""

TEST_HW_PLATFORM := ”HOST”
