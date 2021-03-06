#include "trap.h"
#include "err.h"

.section .text

.global raise
.global jump
.global set_intr
.global set_label
.global func_label
.global goto_label
.global func_label_h
.global drop_to_user
.global fsr_status
.global fault_addr
.global cache_disable
.global cache_enable
.global cache_flush
.global mmu_load_ttb
.global mmu_enable
.global mmu_disable
.global mmu_invalidate
.global cas


raise:
	b . 

	
jump:
	mov sp, r1
	mov pc, r2
	
	
set_intr:
	push {r1, lr}
	mrs r1, cpsr
	cmp r0, #MODE_DI
	orreq r0, r1, #MODE_DI @ disable intr
	bicne r0, r1, #MODE_DI @ or enable intr
	msr cpsr, r0
	and r0, r1, #MODE_DI
	pop {r1, pc}


set_label:
	mrs r1, cpsr
	stmia r0, {r1, sp}
	add r0, r0, #(4 * 7)
	stmia r0, {r4 - r12, lr}

	mov r0, #0
	mov pc, lr
	

goto_label:
	ldmia r0, {r1, sp}
	add r0, r0, #(4 * 7)
	ldmia r0, {r4 - r12, lr}

	mov r0, #1
	msr cpsr, r1
	mov pc, lr


func_label_h:
	pop {r0, r1}
	pop {pc}


drop_to_user:
	cpsid if, #MODE_SYS

	ldmia r0!, {r1, sp, lr}

	cpsid if, #MODE_SVC

	msr spsr, r1
	ldmia r0, {r0 - r12, lr}
	movs pc, lr


fsr_status:
	mrc p15, 0, r0, c5, c0, 0
	mov pc, lr
	
	
fault_addr:
	mrc p15, 0, r0, c6, c0, 0
	mov pc, lr


cache_disable:
	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #(1<<12) @ i cache
	bic r0, r0, #(1<<2) @ d cache
	bic r0, r0, #(1<<11) @ branch prediction
	mcr p15, 0, r0, c1, c0, 0
	mov pc, lr

	
cache_enable:
	mrc p15, 0, r0, c1, c0, 0
	orr r0, r0, #(1<<2) @ unified cache
	mcr p15, 0, r0, c1, c0, 0
	mov pc, lr
	
	
cache_flush:
	mov r0, #0
	mcr p15, 0, r0, c7, c7, 0
	mov pc, lr


mmu_load_ttb:
	;@ tlb base
  mcr p15, 0, r0, c2, c0, 0
  
	;@ domain
	mov r0, #1
  mcr p15, 0, r0, c3, c0, 0

	mov pc, lr
	

mmu_invalidate:
	mov r0, #0
	mcr p15, 0, r0, c8, c7, 0
	mov pc, lr
	

mmu_enable:
	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #(1<<12) @ disable i cache
	bic r0, r0, #(1<< 2) @ disable d cache
	orr r0, r0, #1
	mcr p15, 0, r0, c1, c0, 0
	mov pc, lr
	

mmu_disable:
	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #1
	mcr p15, 0, r0, c1, c0, 0
	mov pc, lr

	
@int
@cas(void *addr, void *old, void *new);
cas:
	ldrex r3, [r0]
	cmp r3, r1
	movne r0, #0
	movne pc, lr
	
	strex r3, r2, [r0]
	cmp r3, #1
	moveq r0, #0
	movne r0, #1
	
	mov pc, lr

