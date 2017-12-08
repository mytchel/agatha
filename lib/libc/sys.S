#include <sysnum.h>

.section .text

.global start
.global raise
.global panic

.global send
.global recv
.global proc_new
.global frame_count
.global frame_info
.global frame_map


start:
	bl main
	b .


raise:
	b .


panic:
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

	
proc_new:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_NEW
	svc 0
	pop {r12, pc}


frame_count:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_COUNT
	svc 0
	pop {r12, pc}
	
	
frame_info:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_INFO
	svc 0
	pop {r12, pc}


frame_map:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_MAP
	svc 0
	pop {r12, pc}


