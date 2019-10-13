#include <sysnum.h>

.section .text

.global _start
.global raise
.global jump
.global panic

.global exit
.global yield
.global pid

.global obj_retype

.global proc_setup
.global proc_start

.global mesg_cap
.global mesg
.global recv_cap
.global recv
.global reply_cap
.global reply
.global signal

.global endpoint_connect

.global intr_connect
.global intr_ack

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
	

proc_setup:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_SETUP
	svc 0
	pop {r12, pc}


proc_start:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_START
	svc 0
	pop {r12, pc}


mesg:
	mov r3, #0
mesg_cap:
	push {r12, lr}
	mov r12, #SYSCALL_MESG
	svc 0
	pop {r12, pc}
	
recv:
	mov r3, #0
recv_cap:
	push {r12, lr}
	mov r12, #SYSCALL_RECV
	svc 0
	pop {r12, pc}

	
reply:
	mov r3, #0
reply_cap:
	push {r12, lr}
	mov r12, #SYSCALL_REPLY
	svc 0
	pop {r12, pc}

	
signal:
	push {r12, lr}
	mov r12, #SYSCALL_SIGNAL
	svc 0
	pop {r12, pc}

	
obj_retype:
	push {r12, lr}
	mov r12, #SYSCALL_OBJ_RETYPE
	svc 0
	pop {r12, pc}


endpoint_connect:
	push {r12, lr}
	mov r12, #SYSCALL_ENDPOINT_CONNECT
	svc 0
	pop {r12, pc}


intr_connect:
	push {r12, lr}
	mov r12, #SYSCALL_INTR_CONNECT
	svc 0
	pop {r12, pc}


intr_ack:
	push {r12, lr}
	mov r12, #SYSCALL_INTR_ACK
	svc 0
	pop {r12, pc}


