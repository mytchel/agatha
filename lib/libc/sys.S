#include <sysnum.h>

.section .text

.global _start
.global raise
.global jump
.global panic

.global yield
.global send
.global recv
.global proc_new
.global va_table

_start:
	bl main
	b .


raise:
	b .

jump:
	mov pc, r0	

panic:
  b . 


yield:
	push {r12, lr}
	mov r12, #SYSCALL_YIELD
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


va_table:
	push {r12, lr}
	mov r12, #SYSCALL_VA_TABLE
	svc 0
	pop {r12, pc}

	
proc_new:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_NEW
	svc 0
	pop {r12, pc}


