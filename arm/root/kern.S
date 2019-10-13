#include <sysnum.h>

.section .text

.global kern_debug
.global intr_create
.global obj_create


kern_debug:
	push {r12, lr}
	mov r12, #SYSCALL_DEBUG
	svc 0
	pop {r12, pc}


intr_create:
	push {r12, lr}
	mov r12, #SYSCALL_INTR_CREATE
	svc 0
	pop {r12, pc}


obj_create:
	push {r12, lr}
	mov r12, #SYSCALL_OBJ_CREATE
	svc 0
	pop {r12, pc}

