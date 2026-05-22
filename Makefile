obj-m +=  mpu6050_driver.o
KDIR := ~/kernel-dev/build_arm64 # Change this one 

CROSS_COMPILE := aarch64-linux-gnu-
ARCH := arm64

PWD := $(shell pwd)

all:
		$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
		$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean

install:
		$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules_install
