# Agatha

A micro kernel.

### Inspired by

	- L4
	- Barrelfish OS
	- Minix
	- Plan9

### Features
	
  - No shared memory.
  - Synchronous message passing.
  - User level control over virtual memory.

## Supported Hardware

  - BeagleBone Black with very minimal support for 
    devices.

The next device to port to will either be a Raspberry Pi
virtual machine/hardware or a odroid. I am trying to make
the kernel as portable as possible by using linux flattened
device tree's to find devices. The same for drivers.

## Trying it out

You will need:
	- an arm cross compiler. I'm using arm-none-eabi
	6.3.1 on OpenBSD. 
	- GNU Make

Then it should be as simple as typing `make`/`gmake`.
You will need to edit `Makefile` if you are using a 
different compiler.

Now the kernel should be built and bundled for U-Boot
at `arm/kern.umg`. Copy that across to your device
somehow along with a device tree blob such as 
`arm/boot/bbb.dtb` for the beagle bone black and use
U-Boot to load the kernel.

Currently all it does it write "Hello 1", "Hello 2", etc
to UART.

## Booting

U-Boot is used to load the kernel and device tree blob.
The kernel then creates proc0 that is linked into it.
proc0 then starts drivers and other processes that were
linked with it. 

Initially proc0 has frames for its code, pages tables,
stack, and a page of information from the kernel. From
here it creates frames for the device tree blob. Mapp's
and reads the blob to find memory which is creates frames
for. Then it starts the linked processes and listens 
for requests. The started processes send messages to proc0
requesting devices based on compatability strings. proc0
looks through the device tree blob, finds the first 
compatable device and gives the requesting proc a frame
for the registers.

## Virtual Memory

Each processses virtual memory is managed by the process
in the form of frames which are either tables memory mapped
io or memory. A process can look through it's frames and 
find it's L1 and L2 frames. Here are the related system
calls.

For more information about how it ended up like this
look [here](docs/memory.md). It is messy but contains
a fair amount of raw reasoning.

```

/* Can only be called by proc0. */
int
frame_create(size_t start, size_t len, int type);

/* Turns a frame into a table of type.
   Must already be mapped read only in the
   current virtual space. */
int
frame_table(int f_id, int type);

/* Mapps pages, sections or tables into the table
   frame t_id. */
int
frame_map(int t_id, int f_id, void *va, int flags);

/* Returns the number of frames the process currently
   has. */
int
frame_count(void);

/* Fills out f with a copy of the i'th frame. */
int
frame_info_index(struct frame *f, int i);

/* Fills out f with the frame of id f_id. */
int
frame_info(struct frame *f, int f_id);

/* Splits the frame f_id at offset. Returns
   the id of the new frame made from the top
   section. */
int
frame_split(int f_id, size_t offset);

/* Merges two frames that must be adjacent. */
int
frame_merge(int a_if, int b_id);

/* Gives a frame to another process. */
int
frame_give(int pid, int f_id);

/* Creates a new process using the frame f_id 
   as it's virtual space. Gives f_id to the new
   process along with any frames mapped into f_id
   either as tables or as pages/sections. */
int
proc_new(int f_id);

```

## Device Drivers

Device drivers are the same as normal processes.
They communicate with proc0 to find registers and
interrupts described in the device tree blob given
to Agatha by U-Boot. 

## IPC

Processes can send unstructued messages
of 64 bytes to any other processs with its process
id as:

```

int
send(int pid, uint8_t *m);

int
recv(uint8_t *m);

```

The process will block on send until the process
it has sent to receivces the message and wake it.

The process will block on recv is there are no
processes waiting to send to it.



