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
#include <sysnum.h>

int
send(proc_t p, uint8_t *m)
{
	debug("%i ksend to %i\n", up->pid, p->pid);
	
	if (p->state == PROC_dead) {
		debug("%i is dead\n", p->pid);
		return ERR;
	} else if (p->m_from != -1) {
		return ERR;
	} else {
		p->m_from = up->pid;
		memcpy(p->m, m, MESSAGE_LEN);
		
		if (p->state == PROC_recv) {
			p->state = PROC_ready;
			schedule(p);
		}
		
		return OK;
	}
}

int
recv(uint8_t *m)
{
	int pid;
	
	debug("%i krecv\n", up->pid);
	
	while (true) {
		pid = up->m_from;
		if (pid != -1) {
			memcpy(m, up->m, MESSAGE_LEN);
			up->m_from = -1;
			return pid;
			
		} else {
			debug("going to sleep\n");
			up->state = PROC_recv;
			schedule(nil);
			debug("%i has been woken up\n", up->pid);
		}
	}
}

reg_t
sys_send(int pid, uint8_t *m)
{
	proc_t p;
	
	debug("%i send to %i\n", up->pid, pid);
	
	p = find_proc(pid);
	if (p == nil) {
		debug("didnt find %i\n", pid);
		return ERR;
	}

	return send(p, m);
}

reg_t
sys_recv(uint8_t *m)
{
	debug("%i recv to 0x%h\n", up->pid, m);
	
	return recv(m);
}

reg_t
sys_proc_new(int f_id)
{
	debug("%i called sys proc_new with %i\n", up->pid, f_id);
	
	return ERR;
}

reg_t
sys_frame_split(int f_id, size_t offset)
{
	debug("%i called sys frame_split with %i, 0x%h\n", up->pid, f_id, offset);
	
	return ERR;
}

reg_t
sys_frame_merge(int f1, int f2)
{
	debug("%i called sys frame_merge with %i, %i\n", up->pid, f1, f2);
	
	return ERR;
}

reg_t
sys_frame_give(int f_id, int pid)
{
	debug("%i called sys frame_give with %i, %i\n", up->pid, f_id, pid);
	
	return ERR;
}

reg_t
sys_frame_map(int pid, int f_id, 
          void *addr, int flags)
{
	debug("%i called sys frame_map with %i, %i, 0x%h, %i\n", 
		up->pid, pid, f_id, addr, flags);
	
	return ERR;
}

reg_t
sys_frame_unmap(int pid, int f_id)
{
	debug("%i called sys frame_unmap with %i, %i\n", up->pid, pid, f_id);
	
	return ERR;
}

reg_t
sys_frame_allow(int pid_allowed_to_map)
{
	debug("%i called sys frame_allow with %i\n", up->pid, pid_allowed_to_map);
	
	return ERR;
}

reg_t
sys_frame_count(void)
{
	debug("%i called sys frame_count\n", up->pid);
	
	return ERR;
}

reg_t
sys_frame_info(int frame_number, struct frame *f)
{
	debug("%i called sys frame_info with %i, 0x%h\n", up->pid, frame_number, f);
	
	return ERR;
}

void *systab[NSYSCALLS] = {
	[SYSCALL_SEND]             = (void *) &sys_send,
	[SYSCALL_RECV]             = (void *) &sys_recv,
	[SYSCALL_PROC_NEW]         = (void *) &sys_proc_new,
	[SYSCALL_FRAME_SPLIT]      = (void *) &sys_frame_split,
	[SYSCALL_FRAME_MERGE]      = (void *) &sys_frame_merge,
	[SYSCALL_FRAME_GIVE]       = (void *) &sys_frame_give,
	[SYSCALL_FRAME_MAP]        = (void *) &sys_frame_map,
	[SYSCALL_FRAME_UNMAP]      = (void *) &sys_frame_unmap,
	[SYSCALL_FRAME_ALLOW]      = (void *) &sys_frame_allow,
	[SYSCALL_FRAME_COUNT]      = (void *) &sys_frame_count,
	[SYSCALL_FRAME_INFO]       = (void *) &sys_frame_info,
};
	