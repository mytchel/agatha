# Agatha

A micro kernel.

### Inspired by

	- QNX
	- L4
	- Barrelfish OS
	- Minix
	- Plan9

### Features
	
  - Synchronous message passing.
  - User level control over virtual memory.

## Supported Hardware

It doesn't do much at the moment but it should start up and
show you that it isn't doing much on the following devices:

  - BeagleBone Black
    devices.
  - qemu vexpress-a9 

## Trying it out

You will need:
	- an arm cross compiler. I'm using arm-none-eabi
	6.3.1 on OpenBSD. 
	- BSD or GNU Make

Then go to the directory of the board you want to build
and run make.

```
cd arm/virt
make
```

The kernel should be built and bundled for U-Boot
at `arm/$board/kern.umg`. Copy that across to your device
somehow along use U-Boot to load the kernel.

## Booting

U-Boot is used to load the kernel.
The kernel then creates proc0 that is linked into it.
proc0 starts drivers and other processes that were
linked along with the kernel as a bundle.

## Virtual Memory

Virtual memory is managed in user space by proc0 which
tells the kernel what L1 page table to use for each process.
So far not very much is implemented.

