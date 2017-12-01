# The Currently Unnamed Kernel

### Inspired by
	- L4
	- Barrelfish OS
	- Plan9
	- Minix


### Features
	
No shared memory.
Asynchronous message passing (for now synchronous).
User level control over virtual memory.
Eventually multi core.

### Virtual Memory

Somehow kernel objects should be provided from user space?
Memory for kernel objects.

Have frames in linked lists. These are given to processes
that can map them however they want. They can also be split
and given to another process.
The kernel has a (set for now, maybe growable in future) buffer
from which to create frame objects. Proc0 starts with all frames
not used by the kernel.

Processes can map parts of frames to a virtual address space by 
some kind of `map_frame(va_space, frame_id, offset, len, addr, flags)`.

Virtual address spaces have to be created in advance.

Mapping frames may be used to add sub tables to the address
space for layered mmu's.

Address space's are created from users current memory? It would
be nice for the process to look through it's address space so yes,
they are made from memory. 

Frames can be mapped as many times as wanted in as many ways as
wanted? Except if it is mapped as a mmu table it would need to be
read only. mmu table mappings could have to be done using an entire
frame that is not mapped anywhere. Then the process can map it read
only afterwards. 

There needs to also be a list of frame->address space mappings that
the kernel can look through. Otherwise it would have to go through
the entire address space looking for frame mappings when frames are
given away.

So that's two kernel linked lists and one layered user structure
managed by the kernel.

Mapping structures could be from user memory, managed by the kernel.
`add_mapping_space(start, len)` which changes the mappings from start
to start+len to be read only and tells the kernel about it.

But frame structures need to be kernel side so they can be swapped
between processes.

Rather than virtual address space objects, each process has one
given to it upon creation (by creator). Processes (creator of child
by default) can modify other processes address spaces. Give them a
frame then map it however.

Giving frames does not require consent of the reciever. Also doesn't
require them to do anything with it. Well, maybe it should. It is not
a good idea to let a process dump a collection of small frames to a
process that goes through its frames at any point.

Only some processes can give a process a frame. Parent, sibling, or
specially allowed processes.

So far we are looking at something like

```

/* Turns a frame into a process. Frame will need to be large
   enough to store the process structure and the address space
   structure. Two pages.
   */
int proc_new(int f_id);

struct frame {
	int f_id, type;
	size_t addr, len;
	
	struct segment *segments;
	struct frame *next;
};

struct segment {
	int s_id, f_id;
	size_t offset, len;
	void *va;
	
	struct segment *next;   /* Full list of segments. */
	struct segment *f_next; /* Segments from the same frame. */
};

/* Returns f_id of upper frame that was created. */
int frame_split(int f_id, size_t offset);

/* Returns f_id of frame in other process. */
int frame_give(int f_id, int pid);

/* For if you are mapping these pages as tables rather than 
   mappings. */
#define F_MAP_MMU_TABLE (1<<0)
#define F_MAP_READ      (1<<1)
#define F_MAP_WRITE     (1<<2)

/* Returns the segment id. */

int frame_map(int pid, int f_id, 
              size_t offset, size_t len, 
              void *addr, int flags);

int segment_unmap(int pid, int s_id);

/* What pid is allowed to give frames, segment space, and map
   to me currently, plus parent and siblings?
 */
int frame_allow(int pid_allowed_to_map);

/* To look through frames. */

int frame_count(void);

int frame_info(int frame_number, struct frame *f);

/* Turns a frame into space for segment structures. 
   Can be mapped read only for viewing. */
int frame_segment_space(int pid, int f_id);


```

But how will the lists work? If the user can map the segment
frames should they be in user mappings or kernel mappings? 

Maybe no segments is the way to go. Only frames that you map in
their entirety?

```

/* Turns a frame into a process. Frame will need to be large
   enough to store the process structure and the address space
   structure. Two pages.
   */
int proc_new(int f_id);

struct frame {
	int f_id;
	
	int type;
	size_t pa, len;
	
	/* Mapping if mapped. */
	size_t va;
	int flags;
	
	/* Not visible from user space. */
	struct frame *next;
};

/* Returns f_id of upper frame that was created. */
int frame_split(int f_id, size_t offset);

/* Must be adjacent frames. */
int frame_merge(int f1, int f2);

/* Returns f_id of frame in other process. */
int frame_give(int f_id, int pid);

/* For if you are mapping these pages as tables rather than 
   mappings. */
#define F_MAP_MMU_TABLE (1<<0)
#define F_MAP_READ      (1<<1)
#define F_MAP_WRITE     (1<<2)

/* Returns the segment id. */

int frame_map(int pid, int f_id, 
              void *addr, int flags);

int frame_unmap(int pid, int f_id);

/* What pid is allowed to give frames, segment space, and map
   to me currently, plus parent? Probably not
   parent once the process is running.
 */
int frame_allow(int pid_allowed_to_map);

/* To look through frames. */

int frame_count(void);

int frame_info(int frame_number, struct frame *f);

```

Ok. Now we are getting somewhere. But with this arangement 
the kernel needs access to the complete address space of
ram so that it can edit the page tables.

How about we force the user to map them somewhere as read
only. Then the kernel has a way to access them. Something like:

```

/* Create a table

#define F_TABLE_L1   1
#define F_TABLE_L2   2

int frame_table(int f_id, void *m_addr, int type);

/* Permitions. */
#define F_MAP_READ      (1<<0)
#define F_MAP_WRITE     (1<<1)

/* Kind of mapping. */
#define F_MAP_TABLE     (0<<2)
#define F_MAP_PAGE      (1<<2)
#define F_MAP_SECTION   (2<<2)

int frame_map(int f_id, void *va, int flags);

```

How processes can set up other processes mappings can be figured
out later on.
