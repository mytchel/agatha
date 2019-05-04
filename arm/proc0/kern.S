#include <sysnum.h>

.section .text

.global proc_new
.global va_table
.global intr_register
.global kern_debug


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


intr_register:
	push {r12, lr}
	mov r12, #SYSCALL_INTR_REGISTER
	svc 0
	pop {r12, pc}


kern_debug:
	push {r12, lr}
	mov r12, #SYSCALL_DEBUG
	svc 0
	pop {r12, pc}




