# The System

How does it all fit together?

Or rather, how should it all fit together?

### IPC

Message based. Asynchronous to work nicely with multi-core
and distributed systems. Channel based? A channel must be
opened somehow before messages can be sent. This will make
different channel types easier (ipc between the same core,
different cores, shared memory, no shared memory, etc). Some
way of restricting what a process can talk to would be good.
And being able to redirect it. L4's clan and chief could work,
though I read somewhere it was undesierable. 

I will need to research how L4 and barrelfish do this. Also 
minix.

#### L4

#### Barrelfish

#### Minix


