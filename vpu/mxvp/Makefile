obj-m := mxvp.o
ccflags-y += -Wall -Wno-unused-function -Werror
mxvp-objs := mxvp_mem.o mxvp_queue.o mxvp_bspec.o mxvp_cmd.o mxvp_pci.o mxvp_main.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf *.o.*

help:
	@echo ""
	@echo "make clean -> delete build artifacts"
	@echo "make all   -> build Myriad-X VPU driver"
	@echo ""

TEST_HW_PLATFORM := ”HOST”
