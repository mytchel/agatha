#include <sysnum.h>

.section .text

.global proc_new
.global proc_setup
.global kern_debug
.global intr_create


proc_new:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_NEW
	svc 0
	pop {r12, pc}


proc_setup:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_SETUP
	svc 0
	pop {r12, pc}


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





