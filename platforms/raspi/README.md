#Platform package for Raspberry Pi 3

##Prerequisites

Raspberry Pi Tools (ARM Cross Compiler).
	
    cd ~/ocpi/opencpi/platforms/raspi
    git clone https://github.com/raspberrypi/tools
    (update ~/opencpi/example_raspi_env.sh)

Raspberry Pi Linux Kernel.

    cd ~/ocpi/opencpi/platforms/raspi
    git clone https://github.com/raspberrypi/linux

##Compile OpenCPI

    cd ~/ocpi/opencpi 
    source ./scripts/clean-env.sh
    source ./example_raspi_env.sh

    source ./scripts/install-prerequisites.sh
    source ./scripts/build-opencpi.sh

##Target Hardware Configuration:

Update Kernel to match prerequisites.

    cd ~
    git clone --depth=1 https://github.com/raspberrypi/linux
    sudo apt-get install bc

    cd linux
    KERNEL=kernel7
    make bcm2709_defconfig

    make -j4 zImage modules dtbs
    sudo make modules_install
    sudo cp arch/arm/boot/dts/*.dtb /boot/
    sudo cp arch/arm/boot/dts/overlays/*.dtb* /boot/overlays/
    sudo cp arch/arm/boot/dts/overlays/README /boot/overlays/
    sudo scripts/mkknlimg arch/arm/boot/zImage /boot/$KERNEL.img

