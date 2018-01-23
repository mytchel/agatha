## IPC

Currently processes can send unstructued messages
of 64 bytes to any other processs with its process
id as:

```

int
send(int pid, uint8_t *m);

int
recv(uint8_t *m);

```

However this is only temporary. 

Now is the time to come up with a better method.

It will be message based. With a fixed message
size but probably not fixed for all messages. 
Fixed per channel perhaps.
And asynchronous.


