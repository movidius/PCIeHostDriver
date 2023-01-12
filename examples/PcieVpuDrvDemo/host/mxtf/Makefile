obj-m := mxtf.o
MXVP_DIR := ../mxvp
ccflags-y += -Wall -Wno-unused-function -Werror -I$(PWD)/$(MXVP_DIR)
mxtf-objs := $(MXVP_DIR)/mxvp_bspec.o \
			 $(MXVP_DIR)/mxvp_cmd.o \
			 $(MXVP_DIR)/mxvp_mem.o \
			 $(MXVP_DIR)/mxvp_pci.o \
			 $(MXVP_DIR)/mxvp_queue.o \

mxtf-objs += mxtf_main.o \
			 mxtf_test.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/$(MXVP_DIR) clean
	rm -rf *.o.*
	rm -rf $(MXVP_DIR)/*.o.*

help:
	@echo ""
	@echo "make clean -> delete build artifacts"
	@echo "make all   -> build Myriad-X test driver"
	@echo ""

TEST_HW_PLATFORM := ”HOST”
