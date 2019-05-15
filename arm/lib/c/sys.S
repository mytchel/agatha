#include <sysnum.h>

.section .text

.global _start
.global raise
.global jump
.global panic

.global exit
.global intr_exit
.global yield
.global send
.global recv
.global mesg
.global pid
.global proc_new
.global va_table
.global intr_register

_start:
	bl main
	bl exit

jump:
	mov sp, r1
	mov pc, r2

raise:
	mov r0, #-1
panic:
	mov r0, #-2
exit:
	push {r12, lr}
	mov r12, #SYSCALL_EXIT
	svc 0
	pop {r12, pc}


intr_exit:
	push {r12, lr}
	mov r12, #SYSCALL_INTR_EXIT
	svc 0
	pop {r12, pc}


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

	
mesg:
	push {r12, lr}
	mov r12, #SYSCALL_MESG
	svc 0
	pop {r12, pc}


pid:
	push {r12, lr}
	mov r12, #SYSCALL_PID
	svc 0
	pop {r12, pc}


