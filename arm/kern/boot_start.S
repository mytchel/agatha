#include "trap.h"
#include <sysnum.h>

.section .text

.global _start

/*
   From U-Boot:
   r2 = device tree blob address.
   r3 = device tree blob size. Consistant strange number if no 
   blob was given.
   r6 = kernel address.

   I have no idea what any of the other registers are set
   for. r1, r5, r9, r12 seem to stay consistant regardless of the
   presense of a device tree. r9 changes.

	These are described somewhere in the u-boot code. I think
	to do with booting linux which agatha is pretending to be.
 */


_start:
	mov r9, r0

	mov r0, #0
	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #(1<< 0) @ disable mmu
	bic r0, r0, #(1<< 2) @ disable d cache
	bic r0, r0, #(1<< 3) @ disable write buffer
	bic r0, r0, #(1<<11) @ disable branch prediction
	bic r0, r0, #(1<<12) @ disable i cache
	bic r0, r0, #(1<<26) @ disable unified cache
	mcr p15, 0, r0, c1, c0, 0

	ldr r0, = _bss_start
	ldr r1, = _bss_end
	mov r2, #0
	mov r3, #0
1:
	stmia r0!, {r2, r3}
	cmp r0, r1
	blt 1b

	cps #MODE_IRQ
	ldr sp, =_ex_stack_top
	cps #MODE_ABORT
	ldr sp, =_ex_stack_top
	cps #MODE_UND
	ldr sp, =_ex_stack_top
	cps #MODE_SVC
	ldr sp, =_ex_stack_top

	mov r0, r9

	b main

