#include <sysnum.h>

.section .text

.global _start
.global raise
.global jump
.global panic

.global exit
.global exit_r
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


raise:


panic:


exit:
exit_r:
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



