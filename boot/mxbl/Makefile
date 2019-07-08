obj-m := mxbl.o
COMMON_DIR := ../../common
ccflags-y += -Wall -Wno-unused-function -Werror -I$(PWD)/$(COMMON_DIR)
mxbl-objs := $(COMMON_DIR)/mx_boot.o \
			 $(COMMON_DIR)/mx_pci.o \
			 $(COMMON_DIR)/mx_reset.o \
			 $(COMMON_DIR)/mx_utils.o
mxbl-objs += mxbl_main.o \
			 mxbl_core.o \
			 mxbl_char.o \

all:
ifdef BINARY
	$(eval ccflags-y += -DEMBEDDED_BINARY)
	python bin_to_hdr.py $(BINARY)
endif
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf *.o.* mxbl_first_stage_image.h
	rm -rf $(COMMON_DIR)/*.o $(COMMON_DIR)/.*.o.cmd $(COMMON_DIR)/.cache.mk

help:
	@echo ""
	@echo "make clean -> delete build artifacts"
	@echo "make all   -> build mxbl driver with char interface"
	@echo "make all BINARY=path/to/binary -> build mxbl driver with embedded binary"
	@echo ""

TEST_HW_PLATFORM := ”HOST”
