int
intr_connect(int cid, int eid, uint32_t signal);

int
intr_ack(int cid);

/* Need to be able to alter l2 for an l1 that is not the
   current procs.
   Need to be able to give an l1 away with all of the frames
   for its capabilities. Possibly give the frame cap's too?
   or have unmap put the memory into a new cap/frame object?
   Mapped frames should only be findable through the table?
   Should the kernel map the l2 table when it needs to edit it?
   It think it will need to.

   Could make it so the cap is invalidated when the page / l2
   is mapped into the l1 and stored in the l1 as a list.
   Then when pages / l2s are unmapped a new cap it fulled
   out pointing to an object popped off the l1's list. Or
   the original object for the page / l2.

   Or we could make it so the caps are kept and not given
   to the new proc with the l1 table. The parent keeps them
   and the child cannot edit what the parent setup for it
   other than adding to it.

  ...

  When a frame is mapped its object is invalidated.
  Then when it is unmapped a cap to an invalid frame object
  is given which is fulled out with the details from the 
  table.

  L1 and L2 tables are mapped on demand for editing.
*/

int
frame_setup(int cid, int type, size_t pa, size_t len);

int
frame_info(int cid, int *type, size_t *pa, size_t *len);

int
frame_l1_setup(int tid);

int
frame_l2_map(int tid, int cid, void *va);

int
frame_l2_unmap(int tid, int nid, void *addr);

int
frame_map(int tid, int cid, void *addr, bool cache);

int
frame_unmap(int tid, int nid, void *addr);

