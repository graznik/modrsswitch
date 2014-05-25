modrsswitch
===========

Raspberry Pi Linux Kernel Module for controlling remote socket switches.

==Usage==
===Configure the build environment===
Use file *setenv.sh* to configure your environment for RPi cross-compiling:

 $ source ./setenv.sh

===Download, configure and build the kernel===
 $ make dl-kernel
 $ make build-kernel

===Compile module===
 $ make

