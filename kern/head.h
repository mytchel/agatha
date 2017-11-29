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

#include <sys.h>
#include <err.h>
#include <types.h>
#include <stdarg.h>
#include <string.h>

typedef struct kframe *kframe_t;

struct kframe {
	struct frame user;
	struct kframe *next;
};

typedef struct proc *proc_t;

typedef enum {
	PROC_dead,
	PROC_enter,
	PROC_ready,
	PROC_oncpu,
	PROC_send,
	PROC_recv,
} procstate_t;

struct proc {
	label_t label;
	
	procstate_t state;
	int pid;
	proc_t next;
	
	int m_from;
	uint8_t m[MESSAGE_LEN];
	
	uint8_t kstack[KSTACK_LEN];
	
	size_t frame_count;
	kframe_t *frames;
	
	vspace_t *vspace;
};

proc_t
proc_new(void);

proc_t
find_proc(int pid);

void
schedule(proc_t next);

int
send(proc_t p, uint8_t *m);

int
recv(uint8_t *m);

kframe_t
frame_split(kframe_t f, size_t offset);

void
memcpy(void *dst, const void *src, size_t len);

void
memset(void *dst, uint8_t v, size_t len);

/* Machine dependant. */

int
debug(const char *fmt, ...);

void
panic(const char *fmt, ...);

void
set_systick(uint32_t ms);

intr_t
set_intr(intr_t i);

int
set_label(label_t *l);

int
goto_label(label_t *l) __attribute__((noreturn));

void
drop_to_user(label_t *l, 
             void *kstack, 
             size_t stacklen)
__attribute__((noreturn));

void
raise(void)
__attribute__((noreturn));

void
func_label(label_t *l, 
           void *stack, 
           size_t stacklen,
           void (*func)(void));


bool
cas(void *addr, void *old, void *new);

/* Variables. */

extern proc_t up;
