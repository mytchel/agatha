#include "trap.h"
#include <sysnum.h>

.section .text

.global _start
_start:
	ldr sp, = _stack_end

	b main


.global vector_table_load
vector_table_load:
	@ Shift vector table to 0x00000000
	@ See page 514 of ARM System Developers Guide
	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #(1 << 13) 		@ set V flag to 0 (disable high vectors)
	mcr p15, 0, r0, c1, c0, 0
	
	ldr r0, =vector_table
	mcr p15, 0, r0, c12, c0, 0	@ set vector base address

	cps #MODE_IRQ	
	ldr sp, = _stack_end
	cps #MODE_ABORT
	ldr sp, = _stack_end
	cps #MODE_UND
	ldr sp, = _stack_end
	cps #MODE_SVC

	bx lr


.balign 32
vector_table:
	ldr pc, =_start
	ldr pc, =undefined_instruction
	ldr pc, =swi_ex
	ldr pc, =prefetch_abort
	ldr pc, =data_abort
	b . @ not assigned
	ldr pc, =irq_ex
	ldr pc, =fiq_ex

swi_ex:
	cmp r12, #NSYSCALLS
	movpls pc, lr
	
	stmfd sp!, {r9, r10, r11, lr}

	mrs r9, spsr

	cpsid if, #MODE_SYS

	mov r10, sp
	mov r11, lr
	
	cpsid if, #MODE_SVC

	ldr lr, =systab
	add r12, lr, r12, lsl #2
	ldr lr, =1f
	ldr pc, [r12]
	
1:
	cpsid if, #MODE_SYS

	mov sp, r10
	mov lr, r11
	
	cpsid if, #MODE_SVC

	msr spsr, r9
	ldmfd sp!, {r9, r10, r11, pc}^

	
irq_ex:
	stmfd sp, {r0 - r4}
	sub r0, lr, #4
	mov r1, #ABORT_INTERRUPT
	b switch
	

undefined_instruction:
	stmfd sp, {r0 - r4}
	sub r0, lr, #4
	mov r1, #ABORT_INSTRUCTION
	b switch


prefetch_abort:
	stmfd sp, {r0 - r4}
	sub r0, lr, #4
	mov r1, #ABORT_PREFETCH
	b switch


data_abort:
	stmfd sp, {r0 - r4}
	sub r0, lr, #8
	mov r1, #ABORT_DATA
	b switch


switch:
	mrs r2, spsr
	sub r4, sp, #(4 * 5)
	
	clrex

	cpsid if, #MODE_SVC

	sub sp, sp, #(4 * 9)
	stmia sp!, {r5 - r12}
	str r0, [sp]
	sub sp, sp, #(4 * 13)

	@ retrieve and store r0 - r4
	ldmfd r4, {r5 - r9}
	stmia sp, {r5 - r9}
	
	ands r3, r2, #0xf
	beq _user

	@ kernel mode interrupted

	push {r2, lr}

	@ r0 -> interrupted pc, r1 -> type
	bl trap

	pop {r2, lr}
	msr spsr, r2
	ldmia sp!, {r0 - r12, pc}^

_user:  @ user mode interrupted

	mov r3, sp
	cpsid if, #MODE_SYS
	stmfd r3!, {r2, sp, lr}
	cpsid if, #MODE_SVC
	mov sp, r3

	@ r0 -> interrupted pc, r1 -> type
	bl trap

	mov r3, sp
	cpsid if, #MODE_SYS
	ldmfd r3!, {r2, sp, lr}
	cpsid if, #MODE_SVC
	mov sp, r3

	msr spsr, r2
	ldmia sp!, {r0 - r12, pc}^


@ Not used. But just in case kick and scream.
fiq_ex:
	b .

