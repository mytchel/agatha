#
#  Copyright (c) 2017 Mytchel Hammond <mytch@lackname.org>
#  
#  Permission is hereby granted, free of charge, to any person
#  obtaining a copy of this software and associated documentation
#  files (the "Software"), to deal in the Software without
#  restriction, including without limitation the rights to use,
#  copy, modify, merge, publish, distribute, sublicense, and/or
#  sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following
#  conditions:
#  
#  The above copyright notice and this permission notice shall be
#  included in all copies or substantial portions of the Software.
#  
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
#  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
#  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
#  OTHER DEALINGS IN THE SOFTWARE
#

#include <syscalls.h>

.section .text

.global start
.global raise

.global get_proc_page
.global get_kernel_page

.global send
.global recv
.global reply
.global reply_recv

.global proc_create

.global addr_offer
.global addr_accept

start:
	bl main
	b .

raise:
	b .

get_proc_page:
	push {r12, lr}
	mov r12, #SYSCALL_GET_PROC_PAGE
	svc 0
	pop {r12, pc}


get_kernel_page:
	push {r12, lr}
	mov r12, #SYSCALL_GET_KERNEL_PAGE
	svc 0
	pop {r12, pc}


send:
	push {r12, lr}
	mov r12, #SYSCALL_SEND
	svc 0
	pop {r12, pc}
	
	
recv:
	push {r12, lr}
	mov r12, #SYSCALL_RECV
	svc 0
	pop {r12, pc}
	
	
reply:
	push {r12, lr}
	mov r12, #SYSCALL_REPLY
	svc 0
	pop {r12, pc}


reply_recv:
	push {r12, lr}
	mov r12, #SYSCALL_REPLY_RECV
	svc 0
	pop {r12, pc}
	
	
proc_create:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_CREATE
	svc 0
	pop {r12, pc}

		
addr_offer:
	push {r12, lr}
	mov r12, #SYSCALL_ADDR_OFFER
	svc 0
	pop {r12, pc}
		
addr_accept:
	push {r12, lr}
	mov r12, #SYSCALL_ADDR_ACCEPT
	svc 0
	pop {r12, pc}
