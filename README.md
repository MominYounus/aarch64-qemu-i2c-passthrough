# AArch64 Virtualized I2C Sensor Driver

A full-stack embedded Linux project demonstrating how to bridge a physical I2C hardware sensor (MPU-6050/6500) into a virtualized AArch64 QEMU environment, complete with a custom Linux character device driver to read live accelerometer data.

## Architecture Overview
This project traverses multiple abstraction layers to connect user-space in a virtual machine to physical silicon on a desk:
1. **Hardware:** MPU-6500 IMU connected via I2C to a QinHeng CH341A USB programmer.
2. **Host:** x86_64 Linux machine passing the raw USB device to the hypervisor.
3. **Hypervisor:** QEMU (`virt` machine) mapping the USB device to a virtual PCIe XHCI controller.
4. **Guest OS:** Custom-compiled AArch64 Linux kernel (v6.6).
5. **Master Driver:** Cross-compiled `ch341-mfd` and `i2c-ch341` modules driving the USB-to-I2C bus.
6. **Client Driver:** Custom `mpu6050_driver.c` utilizing the SMBus API and `miscdevice` framework to expose data to `/dev/mpu_sensor`.

## Prerequisites
* **Hardware:** CH341A USB Programmer & MPU-6050/6500 Sensor.
* **Toolchain:** `aarch64-linux-gnu-` cross-compiler.
* **Hypervisor:** QEMU (`qemu-system-aarch64`).
* **Kernel:** A custom ARM64 kernel compiled with `CONFIG_I2C_CHARDEV`, `CONFIG_PCI`, and `CONFIG_USB_XHCI_HCD`.

## Execution Pipeline

### 1. Cross-Compile the Modules
First, compile the custom client driver against your AArch64 kernel build directory:
```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
```

### 2. Boot the QEMU Guest
Boot the guest environment, ensuring the physical USB device is passed through to the virtualized XHCI controller
```bash
sudo qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a53 \
  -smp 1 \
  -nographic \
  -device qemu-xhci \
  -device usb-host,vendorid=0x1a86,productid=0x5512 \
  -kernel build_arm64/arch/arm64/boot/Image \
  -initrd rootfs_arm64.cpio \
  -append "console=ttyAMA0"
```

### 3. Load the Master Drivers
Note: This project utilizes the excellent ch341-mfd driver by Frank Zago to handle the core USB-to-I2C bridging. These modules must be cross-compiled for AArch64 and loaded first.
```bash
insmod ch341-core.ko
insmod i2c-ch341.ko
```

### 4. Load the Client Driver & Instantiate
Because the CH341 is dynamically attached via USB, the I2C bus lacks a static Device Tree (DTB) node. We must manually instantiate the devic via sysfs:
```bash
# Load the custom sensor driver
insmod mpu6050_driver.ko

# Force hardware-to-driver binding on I2C adapter 0
echo "mpu6050 0x68" > /sys/class/i2c-adapter/i2c-0/new_device
```

### 5. Read Live Data
Once probed, the driver registers a character device. Read the live X-axis accelerometer data from userspace:
```bash
cat /dev/mpu_sensor
```

# Key Learnings
* I2C subsystem architecture (Adapters vs. Clients).

* Using the Kernel SMBus API (i2c_smbus_read_byte_data, i2c_smbus_write_byte_data) for atomic hardware transactions and repeated starts.

* Managing the VFS boundary using struct file_operations, the miscdevice framework, and copy_to_user().

* Debugging PCIe and XHCI driver bindings in a QEMU virt machine

* IOCTL Implementation: Dynamically changing accelerometer sensitivity ranges from userspace.

* Kernel Workqueues (delayed_work): Asynchronous, process-context hardware polling.

* Concurrency Control: Mutex locking to prevent race conditions during VFS reads and module unloading
