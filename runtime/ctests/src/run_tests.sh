#!/bin/bash

# #####
#
#  This file is part of OpenCPI (www.opencpi.org).
#     ____                   __________   ____
#    / __ \____  ___  ____  / ____/ __ \ /  _/ ____  _________ _
#   / / / / __ \/ _ \/ __ \/ /   / /_/ / / /  / __ \/ ___/ __ `/
#  / /_/ / /_/ /  __/ / / / /___/ ____/_/ / _/ /_/ / /  / /_/ /
#  \____/ .___/\___/_/ /_/\____/_/    /___/(_)____/_/   \__, /
#      /_/                                             /____/
#
#  OpenCPI is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as published
#  by the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  OpenCPI is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with OpenCPI.  If not, see <http://www.gnu.org/licenses/>.
#
########################################################################### #

# quick hack to run all tests
# run this in the binary executables directory by doing: ../bin/src/run_tests.sh
export OCPI_RCC_TARGET=$OCPI_TARGET_HOST
export OCPI_SMB_SIZE=3000000
export OCPI_LIBRARY_PATH=$OCPI_CDK_DIR/lib/components
if test "$OCPI_TARGET_OS" = macos; then
  export OCPI_RCC_SUFFIX=dylib
else
  export OCPI_RCC_SUFFIX=so
fi
tmp=/tmp/ocpictest$$
failed=
if [ -n "$BASH" ]; then
  set -o pipefail
fi
out="2> /dev/null"
if test "$OUT" != ""; then out="$OUT"; fi
function doit {
  $VG ./$1 $out | tee $tmp | (egrep 'FAILED|PASSED|Error:';exit 0)
  rc=$?
  if egrep -q 'FAILED|Error:' $tmp; then
     echo $1 test failed explicitly, with FAILED or Error message.
     if test "$failed" = ""; then failed=$1; fi
  elif test $rc != 0; then
     echo $1 test failed implicitly, no FAILED message, but non-zero exit.
     if test "$failed" = ""; then failed=$1; fi
  elif ! grep -q PASSED $tmp; then
     echo $1 test failed implicitly, no PASSED message, but zero exit.
     if test "$failed" = ""; then failed=$1; fi
  fi
}
if test $# = 1 ; then
  doit $1
else
for i in `ls -d test* | grep -v '_main' | grep -v '\.'` ; do
  echo Running $i...
  doit $i
done
fi
if test "$failed" = ""; then
  echo All container tests passed.
  exit 0
else
  echo Some container tests failed.  The first to fail was: $failed.
  exit 1
fi


