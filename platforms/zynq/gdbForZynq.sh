#!/bin/bash
# 1. First on the target system run:  gdbserver host:1234 <command> <args>
# 2. Run this script on the dev system with zed dev env: zedgdb <mainprogram> ip-addr-of-zed
# 3. Use the "set directories" command to point to source directories
# 4. c

main=$1
shift
addr=$1
shift
# assume the current target is a zynq target
source $OCPI_CDK_DIR/scripts/ocpitarget.sh zed
exec $OCPI_CROSS_BUILD_BIN_DIR/$OCPI_CROSS_HOST-gdb \
  $main \
  -iex "set sysroot $OCPI_CDK_DIR/platforms/zed/release/uramdisk/root" \
  -iex "set solib-search-path  $OCPI_CDK_DIR/platforms/zed:$OCPI_CDK_DIR/lib/$OCPI_TARGET_DIR:$OCPI_CDK_DIR/lib/components/rcc/$OCPI_TARGET_DIR" \
  -iex "target remote ${addr}:1234" \
  $*