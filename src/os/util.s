/*
 * intr.s
 *
 *  Created on: 2022/08/13
 *      Author: User
 */
   .syntax unified
	.cpu cortex-m4
	.fpu softvfp
	.thumb

.global	vec_common

//.extern interrput
    .section	.text.vec_common
	.weak	vec_common
	.type	vec_common, %function
vec_common:
  push {r4-r11,LR}  /* Save all registers that are not saved */
  MRS   r0, IPSR    /* first argument = interrupt no         */
  MRS   r1, MSP     /* second argument = stack pointer       */
  bl    thread_intr /* jump to thread_intr                   */
  pop  {r4-r11,PC}

