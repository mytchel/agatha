#include <sysnum.h>

.section .text

.global _start
.global raise
.global jump
.global panic

.global exit
.global intr_exit
.global yield
.global mesg
.global recv
.global reply
.global signal
.global endpoint_create
.global endpoint_offer
.global endpoint_accept
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


yield:
	push {r12, lr}
	mov r12, #SYSCALL_YIELD
	svc 0
	pop {r12, pc}

	
pid:
	push {r12, lr}
	mov r12, #SYSCALL_PID
	svc 0
	pop {r12, pc}


intr_exit:
	push {r12, lr}
	mov r12, #SYSCALL_INTR_EXIT
	svc 0
	pop {r12, pc}


mesg:
	push {r12, lr}
	mov r12, #SYSCALL_MESG
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

	
signal:
	push {r12, lr}
	mov r12, #SYSCALL_SIGNAL
	svc 0
	pop {r12, pc}
	

endpoint_create:
	push {r12, lr}
	mov r12, #SYSCALL_ENDPOINT_CREATE
	svc 0
	pop {r12, pc}


endpoint_offer:
	push {r12, lr}
	mov r12, #SYSCALL_ENDPOINT_OFFER
	svc 0
	pop {r12, pc}


endpoint_accept:
	push {r12, lr}
	mov r12, #SYSCALL_ENDPOINT_ACCEPT
	svc 0
	pop {r12, pc}


