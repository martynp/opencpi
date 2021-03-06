#!/bin/sh
# Run all the go-no-go tests we have
set -e
if test "$OCPI_CDK_DIR" != ""; then
  echo Since OCPI_CDK_DIR is set, we will use the existing environment.
else
  # We're being run in an uninitialized environment
  if test ! -d env; then
    echo It appears that this script is not being run at the top level of OpenCPI.
    exit 1
  fi
  OCPI_BOOTSTRAP=`pwd`/exports/scripts/ocpibootstrap.sh; . $OCPI_BOOTSTRAP
  test $? = 0 || exit 1; 
fi
echo ======================= Loading the OpenCPI Linux Kernel driver. &&
(test "$OCPI_TOOL_OS" = macos || $OCPI_CDK_DIR/scripts/ocpidriver load) &&
echo ======================= Running Unit Tests &&
tests/target-$OCPI_TOOL_DIR/ocpitests &&
echo ======================= Running Datatype/protocol Tests &&
tools/cdk/ocpidds/target-$OCPI_TOOL_DIR/ocpidds -t 10000 > /dev/null &&
echo ======================= Running Container Tests &&
(cd runtime/ctests/target-$OCPI_TOOL_DIR && ${OCPI_TOOL_MODE:+../}../src/run_tests.sh) &&
echo All tests passed.
