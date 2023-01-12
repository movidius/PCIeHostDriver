MDK_INSTALL_DIR ?= ../../../../../common

include $(MDK_INSTALL_DIR)/../system/build/mdk.mk

TEST_HW_PLATFORM   := "MV0262_MA2480"
TEST_TYPE          := "MANUAL"
TEST_TAGS := "MDK_DRIVERS, MA2480, WIN_MX"
