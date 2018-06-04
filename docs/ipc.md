## IPC

Currently processes can synchronously send 
unstructured messages of 64 bytes to any other 
process with its process id as:

```

int
send(int pid, uint8_t *m);

int
recv(uint8_t *m);

```

However this is only temporary. 

### Thoughts

Messages should have some structure. Such as who
they are from, and who they are for which may be
different to who they were sent to in the case of
forwarding between cpus(?) and machines. 

In general they should be synchronous but there are
cases where asynchronous would make life a lot simpler.
So synchronous built on top of a asynchronous system.
This means there will be some copying but such is life.

They will have a fixed size. The kernel may have a limit
on how many messages can be in transit at one time.
With this we could have a linked list of messages that
get taken on send, fulled out and given to the receiving
process which reads the contents to itself then puts
the container back on the free list. It may be helpful 
to have some way to only receive a message from a process
the process is waiting for.

On top of this most user space calls could be a send
receive combination that picks out the reply from the
process it just sent to. 

```

/* Asynchronous send/receive to/from a process */
int
send(int pid, uint8_t *s);

/* pid = -1 for receive from any process */
int
recv(int pid, uint8_t *r);

/* Synchronous send and receive to and from a process.
   This will be used most of the time. */
int
msg(int pid, uint8_t *s, uint8_t *r);

```

After that it might be made that each process
has an process page/structure which the kernel knows
about and that is where messages are written/read 
to/from so that the kernel doesn't have to worry
about address's given to it.

