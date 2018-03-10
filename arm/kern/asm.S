#
#  Copyright (c) 2017 Mytchel Hammond <mytch@lackname.org>
#  
#  Permission is hereby granted, free of charge, to any person
#  obtaining a copy of this software and associated documentation
#  files (the "Software"), to deal in the Software without
#  restriction, including without limitation the rights to use,
#  copy, modify, merge, publish, distribute, sublicense, and/or
#  sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following
#  conditions:
#  
#  The above copyright notice and this permission notice shall be
#  included in all copies or substantial portions of the Software.
#  
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
#  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
#  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
#  OTHER DEALINGS IN THE SOFTWARE
#

#include "trap.h"
#include "err.h"

.section .text

.global raise
.global jump
.global set_intr
.global set_label
.global func_label
.global goto_label
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
	mov pc, r0
	b . 
	
	
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


drop_to_user:
	add r1, r1, r2
	mov sp, r1

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
	@ Disable cache and mmu
	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #(1<<2) @ unified cache
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

