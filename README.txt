modrsswitch
===========

Raspberry Pi Linux Kernel Module for controlling remote socket switches.

==Usage==
===Intall cross compiler toolchain (x86_64)
 $ git clone https://github.com/raspberrypi/tools
 $ sudo cp -r tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64 /opt/

===Configure the build environment===
Use file *setenv.sh* to configure your environment for RPi cross-compiling:

 $ source ./setenv.sh

===Download, configure and build the kernel===
 $ make dl-kernel
 $ make build-kernel

===Compile module===
 $ make

