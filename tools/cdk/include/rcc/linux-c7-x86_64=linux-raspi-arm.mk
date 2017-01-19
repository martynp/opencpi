raspi_bin_dir=/home/vagrant/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin
raspi_cross_host=arm-linux-gnueabihf
Gc_linux-raspi-arm=$(raspi_bin_dir)/$(raspi_cross_host)-gcc -std=c99
Gc_LINK_linux-raspi-arm=$(Gc_linux-raspi-arm)
Gc++_linux-raspi-arm=$(raspi_bin_dir)/$(raspi_cross_host)-g++
Gc++_LINK_linux-raspi-arm=$(Gc++_linux-raspi-arm)


