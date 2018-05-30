# Agatha

A micro kernel.

### Features
	
- Message passing handled by the kernel for IPC
  and interrupts.
- Virtual memory handled outside of the kernel
  with support from the kernel.
- Userspace device drivers for everything except
  the interrupt controller, and a system timer.
  Currently the kernel also takes a serial device
  for debugging.

### Inspired by

- QNX
- L4
- Barrelfish OS
- Minix
- Plan9

## Supported Hardware

It doesn't do much at the moment but it should start up and
show you that it isn't doing much on the following devices:

- BeagleBone Black
- qemu vexpress-a9 

Currently the kernel is not portable between devices and
relies on a board specific `device.c` file and kernel
device drivers. Each board
needs to have its own kernel and proc0 compiled.

## Trying it out

You will need:
- an arm cross compiler. I'm using arm-none-eabi
	6.3.1 on OpenBSD 6.2. 
- Make. I'm use BSD make but GNU should also work.

Then go to the directory of the board you want to build
and run make. For example to build for vexpress-a9:

```
export CROSS=arm-none-eabi-
cd arm/virt
make
```

The kernel should be built and bundled for U-Boot
at `arm/virt/kern.umg`. Copy that across to your device
somehow along use U-Boot to load the kernel.

To run it I am currently using qemu to emulate, booting
U-Boot which is loading the kernel from dnsmasq's tftp
server. Look at the script `arm/virt/test`. The U-Boot
binary is at `arm/virt/u-boot`.

