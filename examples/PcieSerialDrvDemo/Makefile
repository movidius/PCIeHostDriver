# ------------------------------[ General Settings ]--------------------------------------#
# Set MV_COMMON_BASE relative to mdk directory location (but allow user to override in environment)
MV_COMMON_BASE     ?= ../../../../../common

MV_SOC_PLATFORM     = myriad2
MV_SOC_REV          = ma2480
MV_SOC_OS           = rtems

# ------------------------------[ Components used ]--------------------------------#
ComponentList := FatalExtension

#--------------------------[ Include common makefile rules ]-----------------------#
# Include the generic Makefile
include $(MV_COMMON_BASE)/generic.mk

# ------------------------------[ Test tags ]--------------------------------#
TEST_HW_PLATFORM   := "MV0262_MA2480"
TEST_TYPE          := "MANUAL"
TEST_TAGS := "MDK_DRIVERS, MA2480, WIN_MX"
