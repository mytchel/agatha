### Device Drivers

How should device drivers be started?

Easiest way to get the code in is using `bundle.sh` to
join the bin files together. And starting them from there
isn't hard, but how do they get the memory mapped registers
they need? I'm thinking have a process that is given the
flattened device tree blob and all the registers addresses
found in that. 

Should proc0 be given the registers or another proc?
Should proc0 be able to create it's own frames, not splitting
existing frames but create its own?

Anyway, having a process that reads the device tree blob
and responds to messages about drivers and returns frames
for registers and some other information would be a good start.

Eh. Now I have to figure out how flattened device tree blobs
work.

Eww.





