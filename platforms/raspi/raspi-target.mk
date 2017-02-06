# Setup to build this target
OcpiThisFile=$(lastword $(MAKEFILE_LIST))

f:=$(OCPI_RASPI_TOOL_DIR)/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin
ifeq ($(wildcard $f),)
  $(error When setting up to build for raspi, OCPI_RASPI_TOOL_DIR is "$(OCPI_RASPI_TOOL_DIR)". Cannot find $f.).
endif
export OCPI_CROSS_BUILD_BIN_DIR:=$f
export OCPI_CROSS_HOST:=arm-linux-gnueabihf
export OCPI_TARGET_CFLAFS:=-mcpu=cortex-a53 -Wall -std=c99
export OCPI_TARGET_CXXFLAGS:=-mcpu=cortex-a53 -Wall -std=c++0x
#export OCPI_TARGET_CFLAGS:=-mfpu=neon-fp16 -mfloat-abi=softfp -march=armv7-a -mtune=cortex-a9 -Wall -Wfloat-equal -Wextra -fno-strict-aliasing -Wconversion -std=c99
#export OCPI_TARGET_CXXFLAGS:=-mfpu=neon-fp16 -mfloat-abi=softfp -march=armv7-a -mtune=cortex-a9 -Wall -Wfloat-equal -Wextra -fno-strict-aliasing -Wconversion -std=c++0x
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
  export OCPI_TARGET_KERNEL_DIR=$(OCPI_CDK_DIR)/platforms/raspi/linux
endif
