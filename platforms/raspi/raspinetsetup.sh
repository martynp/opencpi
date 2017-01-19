# Useage
# source raspinetsetup.sh [ip] [nfs share]

# Attempt to make a folder and mount the nfs share
mkdir -p ~/ocpi
sudo mount $1:$2 ~/ocpi

# Set envrionment variables
export OCPI_CDK_DIR=~/ocpi/opencpi/ocpi
export OCPI_TOOL_HOST=linux-raspi-arm
export OCPI_TOOL_DIR=$OCPI_TOOL_HOST
export OCPI_LIBRARY_PATH=$OCPI_CDK_DIR/lib/components/rcc:$OCPI_TOOL_DIR:$OCPI_CDK_DIR/lib/platforms/raspi
export PATH=$OCPI_CDK_DIR/bin/$OCPI_TOOL_DIR:$PATH

# Exports to speed up container discovery
export OCPI_SUPPRESS_HDL_NETWORK_DISCOVERY=1

# Set the System Config file
export OCPI_SYSTEM_CONFIG=~/ocpi/opencpi/platforms/raspi/system.xml

# Zed platform also includes LD path alteration
export LD_LIBRARY_PATH=$OCPI_CDK_DIR/lib/$OCPI_TOOL_DIR:$LD_LIBRARY_PATH

# Load the driver
ocpidriver load

# List containers
ocpirun -C

