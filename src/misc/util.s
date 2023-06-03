/*
 * util.s
 *
 *  Created on: 2023/5/31
 *      Author: User
 */
   .syntax unified
	.cpu cortex-m4
	.fpu softvfp
	.thumb

.global	_busy_wait

.section	.text._busy_wait
.weak	_busy_wait
.type	_busy_wait, %function

_busy_wait:
1:	sub		r0, #1		// r0 = r0 - 1				1cycle
	nop					// no operation				1cycle
	cmp		r0, #0		// if (r0 != 0)				1cycle	
	bne.n	1b			// 「1:」に戻る				1cycle
	bx		lr			// return
