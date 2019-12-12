#include <sysnum.h>

.section .text

.global _start
_start:
	bl main
	bl exit


.global exit
exit:
	push {r12, lr}
	mov r12, #SYSCALL_EXIT
	svc 0
	pop {r12, pc}


.global jump
jump:
	mov sp, r1
	mov pc, r2

.global yield
yield:
	push {r12, lr}
	mov r12, #SYSCALL_YIELD
	svc 0
	pop {r12, pc}

	
.global pid
pid:
	push {r12, lr}
	mov r12, #SYSCALL_PID
	svc 0
	pop {r12, pc}
	

.global proc_setup
proc_setup:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_SETUP
	svc 0
	pop {r12, pc}


.global proc_set_priority
proc_set_priority:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_SET_PRIORITY
	svc 0
	pop {r12, pc}


.global proc_start
proc_start:
	push {r12, lr}
	mov r12, #SYSCALL_PROC_START
	svc 0
	pop {r12, pc}


.global mesg
mesg:
	mov r3, #0
.global mesg_cap
mesg_cap:
	push {r12, lr}
	mov r12, #SYSCALL_MESG
	svc 0
	pop {r12, pc}
	

.global recv
recv:
	mov r3, #0
.global recv_cap
recv_cap:
	push {r12, lr}
	mov r12, #SYSCALL_RECV
	svc 0
	pop {r12, pc}

	
.global reply
reply:
	mov r3, #0
.global reply_cap
reply_cap:
	push {r12, lr}
	mov r12, #SYSCALL_REPLY
	svc 0
	pop {r12, pc}

	
.global signal
signal:
	push {r12, lr}
	mov r12, #SYSCALL_SIGNAL
	svc 0
	pop {r12, pc}

	
.global obj_create
obj_create:
	push {r12, lr}
	mov r12, #SYSCALL_OBJ_CREATE
	svc 0
	pop {r12, pc}


.global obj_retype
obj_retype:
	push {r12, lr}
	mov r12, #SYSCALL_OBJ_RETYPE
	svc 0
	pop {r12, pc}


.global obj_split
obj_split:
	push {r12, lr}
	mov r12, #SYSCALL_OBJ_SPLIT
	svc 0
	pop {r12, pc}


.global obj_merge
obj_merge:
	push {r12, lr}
	mov r12, #SYSCALL_OBJ_MERGE
	svc 0
	pop {r12, pc}


.global endpoint_connect
endpoint_connect:
	push {r12, lr}
	mov r12, #SYSCALL_ENDPOINT_CONNECT
	svc 0
	pop {r12, pc}


.global intr_connect
intr_connect:
	push {r12, lr}
	mov r12, #SYSCALL_INTR_CONNECT
	svc 0
	pop {r12, pc}


.global intr_ack
intr_ack:
	push {r12, lr}
	mov r12, #SYSCALL_INTR_ACK
	svc 0
	pop {r12, pc}


.global frame_setup
frame_setup:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_SETUP
	svc 0
	pop {r12, pc}


.global frame_info
frame_info:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_INFO
	svc 0
	pop {r12, pc}


.global frame_l1_setup
frame_l1_setup:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_L1_SETUP
	svc 0
	pop {r12, pc}



.global frame_l2_map
frame_l2_map:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_L2_MAP
	svc 0
	pop {r12, pc}



.global frame_l2_unmap
frame_l2_unmap:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_L2_MAP
	svc 0
	pop {r12, pc}


.global frame_map
frame_map:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_MAP
	svc 0
	pop {r12, pc}


.global frame_unmap
frame_unmap:
	push {r12, lr}
	mov r12, #SYSCALL_FRAME_UNMAP
	svc 0
	pop {r12, pc}

