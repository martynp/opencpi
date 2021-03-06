# Setup to build this target
OcpiThisFile=$(lastword $(MAKEFILE_LIST))
include $(dir $(OcpiThisFile))/xilinx-target.mk
f:=$(OCPI_XILINX_EDK_DIR)/gnu/arm/lin/bin
ifeq ($(wildcard $f),)
  $(error When setting up to build for zed, OCPI_XILINX_EDK_DIR is "$(OCPI_XILINX_EDK_DIR)". Cannot find $f. Perhaps the EDK was not installed when Xilinx tools were installed?).
endif
export OCPI_CROSS_BUILD_BIN_DIR:=$f
export OCPI_CROSS_HOST:=arm-xilinx-linux-gnueabi
export OCPI_TARGET_CFLAGS:=-mfpu=neon-fp16 -mfloat-abi=softfp -march=armv7-a -mtune=cortex-a9 -Wall -Wfloat-equal -Wextra -fno-strict-aliasing -Wconversion -std=c99
export OCPI_TARGET_CXXFLAGS:=-mfpu=neon-fp16 -mfloat-abi=softfp -march=armv7-a -mtune=cortex-a9 -Wall -Wfloat-equal -Wextra -fno-strict-aliasing -Wconversion -std=c++0x
export OCPI_LDFLAGS=
export OCPI_SHARED_LIBRARIES_FLAGS=
export CC:=$(OCPI_CROSS_BUILD_BIN_DIR)/$(OCPI_CROSS_HOST)-gcc
export CXX:=$(OCPI_CROSS_BUILD_BIN_DIR)/$(OCPI_CROSS_HOST)-c++
export LD:=$(OCPI_CROSS_BUILD_BIN_DIR)/$(OCPI_CROSS_HOST)-c++
export AR:=$(OCPI_CROSS_BUILD_BIN_DIR)/$(OCPI_CROSS_HOST)-ar
export OCPI_EXPORT_DYNAMIC:=-rdynamic
export OCPI_EXTRA_LIBS:=rt dl pthread
ifndef OCPI_TARGET_KERNEL_DIR
  # When we build the driver the kernel should be cloned, checked out
  # with the label consistent with the ISE version, and build there
  export OCPI_TARGET_KERNEL_DIR=$(OCPI_CDK_DIR)/platforms/zed/release/kernel-headers
endif
