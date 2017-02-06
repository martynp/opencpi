trap "trap - ERR; break" ERR; for i in 1; do
. ./env/start.sh

export OCPI_TARGET_PLATFORM=raspi

export OCPI_RASPI_TOOL_DIR=~/tools/

. ./env/finish.sh
done; trap - ERR
