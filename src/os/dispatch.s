/*
 * dispatch.s
 *
 *  Created on: 2022/08/13
 *      Author: User
 */
  .syntax unified
	.cpu cortex-m4
	.fpu softvfp
	.thumb
.global	_dispatch
.global	_dispatch_int

    .section	.text._dispatch
	.weak	_dispatch
	.type	_dispatch, %function
_dispatch:
    MOV   R13, R0
    LDR   R0,  [R13, #0]
	LDR   R1,  [R13, #4]
	LDR   R2,  [R13, #8]
	LDR   R3,  [R13, #12]
	LDR   R4,  [R13, #16]
	LDR   R5,  [R13, #20]
	LDR   R6,  [R13, #24]
	LDR   R7,  [R13, #28]
	LDR   R8,  [R13, #32]
	LDR   R9,  [R13, #36]
	LDR   R10, [R13, #40]
	LDR   R11, [R13, #44]
	LDR   R12, [R13, #48]
	ADD   R13, #52
	pop {pc}

    .section	.text._dispatch_int
	.weak	_dispatch_int
	.type	_dispatch_int, %function
_dispatch_int:
    MOV   R13, R0
    pop {r4-r11,pc}
