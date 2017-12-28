#include <sysnum.h>

.section .text

.global _start
.global raise
.global panic

.global yield
.global send
.global recv
.global proc_new
.global frame_create
.global frame_count
.global frame_info_index
.global frame_info
.global frame_map
.global frame_table
.global frame_split
.global frame_merge
.global frame_give


_start:
	bl main
	b .


raise:
	b .


panic:
  b . 


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

	
proc_new:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_NEW
	svc 0
	pop {r12, pc}


frame_create:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_CREATE
	svc 0
	pop {r12, pc}


frame_count:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_COUNT
	svc 0
	pop {r12, pc}
	
	
frame_info_index:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_INFO_INDEX
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


frame_table:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_TABLE
	svc 0
	pop {r12, pc}


frame_split:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_SPLIT
	svc 0
	pop {r12, pc}


frame_merge:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_MERGE
	svc 0
	pop {r12, pc}


frame_give:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_GIVE
	svc 0
	pop {r12, pc}

