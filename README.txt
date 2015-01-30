modrsswitch
===========

Raspberry Pi Linux Kernel Module for controlling remote socket switches.

==Cross compiling==
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

===Copy to Rasperry Pi===
 $ scp modrss.ko user@rpi:~/

==Usage==
===Load the module===
 # insmod ./modrss.ko

===Switch a socket===
Socket type:   0 (REV 008345)
Socket group:  1
Socket socket: 0
On:    	       1

$ echo "0101" > /dev/rsswitch

Socket type:   0 (REV 008345)
Socket group:  1
Socket socket: 0
Off:           1

$ echo "0100" > /dev/rsswitch
