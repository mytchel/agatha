/*
 *
 * Copyright (c) 2017 Mytchel Hammond <mytch@lackname.org>
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/* Turns a frame into a process. Frame will need to be large
   enough to store the process structure and the address space
   structure. Two pages.
   */
int
proc_new(int f_id);

int
send(int pid, uint8_t *m);

int
recv(uint8_t *m);

/* Returns f_id of upper frame that was created. */
int frame_split(int f_id, size_t offset);

/* Must be adjacent frames. */
int frame_merge(int f1, int f2);

/* Returns f_id of frame in other process. */
int frame_give(int f_id, int pid);

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

bool
cas(void *addr, void *old, void *new);

void
memcpy(void *dst, const void *src, size_t len);

void
memset(void *dst, uint8_t v, size_t len);
