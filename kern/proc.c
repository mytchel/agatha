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

#include <head.h>

void add_to_list(struct proc **, struct proc *);
bool remove_from_list(struct proc **, struct proc *);

static uint32_t nextpid = 0;

static struct proc procs[MAX_PROCS] = { 0 };
static proc_t alive = nil;
proc_t up = nil;

static struct proc *
next_proc(void)
{
	proc_t p;
	
	if (alive == nil) {
		return nil;
	} else if (up != nil) {
		p = up->next;
	} else {
		p = alive;
	}
	
	do {
		if (p == nil) {
			p = alive;
			
		} else if (p->state == PROC_ready) {
			return p;
			
		} else {
			p = p->next;
		}
	} while (p != up);
	
	return (p != nil && p->state == PROC_ready) ? p : nil;
}

void
schedule(proc_t n)
{
	if (up != nil) {
		if (up->state == PROC_oncpu) {
			up->state = PROC_ready;
		}
    
		if (set_label(&up->label)) {
			return;
		}
	}
	
	if (n != nil) {
		up = n;
	} else {
		up = next_proc();
	}
		
	set_systick(1000);

	if (up != nil) {
		up->state = PROC_oncpu;
		goto_label(&up->label);
		
	} else {
		debug("NO PROCS TO RUN!!\n");
		raise();
	}
}

void
add_to_list(proc_t *l, proc_t p)
{
	p->next = *l;
	*l = p;
}

bool
remove_from_list(proc_t *l, proc_t p)
{
	proc_t pt;

	if (*l == p) {
		*l = p->next;
	} else {
		for (pt = *l; pt != nil && pt->next != p; pt = pt->next)
			;
	
		pt->next = p->next;
	}
	
	return true;
}
	
static void
proc_start(void)
{
	uint8_t m[MESSAGE_LEN];
	label_t u;
	int p;
	
	debug("proc_start %i\n", up->pid);
	
	p = recv(m);
	debug("proc_start for %i got started by %i\n", up->pid, p);
	if (p < 0) {
		/* TODO */
		raise();
	}
		
	u.pc = *(((uint32_t *) m));
	u.sp = *(((uint32_t *) m) + 1);
	
	drop_to_user(&u, up->kstack, KSTACK_LEN);
}

proc_t
proc_new(void)
{
  int pid, npid;
  proc_t p;
  
	do {
		pid = nextpid;
		npid = (pid + 1) % MAX_PROCS;
	} while (!cas(&nextpid, 
	              (void *) pid, 
	              (void *) npid));
	
  p = &procs[pid];
	memset(p, 0, sizeof(struct proc));
  
  p->pid = pid;
  p->m_from = -1;
  
	func_label(&p->label, p->kstack, KSTACK_LEN, &proc_start);
	
	p->state = PROC_ready;
	
	add_to_list(&alive, p);
		
  return p;
}

proc_t
find_proc(int pid)
{
  return &procs[pid];
}
