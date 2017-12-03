#include <sysnum.h>

.section .text

.global start
.global raise

.global send
.global recv

start:
	bl main
	b .

raise:
	b .

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
	
