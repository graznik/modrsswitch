# Enable pr_debug() output
CFLAGS_modrss.o := -DDEBUG

ifneq ($(KERNELRELEASE),)
	obj-m := modrss.o

else
	KERNELDIR ?= ~/hackstock/kernelbuild/linux-rpi
	PWD := $(shell pwd)
endif

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

dl-kernel:
	if ! [ -d linux-rpi-3.12.y ]; then wget https://github.com/raspberrypi/linux/archive/rpi-3.12.y.tar.gz && tar -zxvf rpi-3.12.y.tar.gz; fi
	if ! [ -s ./linux ]; then ln -s linux-rpi-3.12.y linux; for i in ./patches/*.patch; do (cd ./linux && patch -p1 < ../$$i); done; fi
	if ! [ -f ./linux/localversion ]; then echo "-modtest" > ./linux/localversion; fi

build-kernel: dl-kernel
	if ! [ -f ./linux/.config ]; then (cd ./linux && make bcmrpi_defconfig); fi
	(cd ./linux && make -j2)

clean-kernel:
	(cd ./linux && make clean)
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	-@rm *~ 2>/dev/null || true
