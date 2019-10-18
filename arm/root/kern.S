#include <sysnum.h>

.section .text

.global kern_debug
.global intr_init


kern_debug:
	push {r12, lr}
	mov r12, #SYSCALL_DEBUG
	svc 0
	pop {r12, pc}


intr_init:
	push {r12, lr}
	mov r12, #SYSCALL_INTR_INIT
	svc 0
	pop {r12, pc}

