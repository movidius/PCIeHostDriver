obj-m := mxbl.o
ccflags-y += -Wall -Wno-unused-function -Werror
mxbl-objs := mxbl_bspec.o mxbl_pci.o mxbl_main.o mxbl_char.o

all:
ifdef BINARY
	$(eval ccflags-y += -DEMBEDDED_BINARY)
	python bin_to_hdr.py $(BINARY)
endif
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules "ccflags-y += $(ccflags-y)"

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf *.o.* mxbl_first_stage_image.h

help:
	@echo ""
	@echo "make clean -> delete build artifacts"
	@echo "make all   -> build mxbl driver with char interface"
	@echo "make all BINARY=path/to/binary -> build mxbl driver with embedded binary"
	@echo ""

TEST_HW_PLATFORM := ”HOST”
