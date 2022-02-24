/* hv vcpu assembly functions
 * @author pwang
 * @mail wtliang110@aliyun.com
 *
 *  * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/


/*********************************************************** 
* vcpu asm operations:
* void hv_arch_vcpu_context_save(struct arch_vcpu *vcpu)
* void hv_arch_vcpu_context_restore(struct arch_vcpu *vcpu)
* void hv_arch_vcpu_launch(void *entry, void *arg) 
************************************************************/

#define __ASSEMBLY__ 
#include <hv_arch_vcpu_defs.h>

/* void hv_arch_vcpu_context_save(struct arch_vcpu *vcpu) */
.global hv_arch_vcpu_context_save
.type hv_arch_vcpu_context_save @function
hv_arch_vcpu_context_save:
	mrs x1, sctlr_el1
	mrs x2, actlr_el1
	stp x1, x2, [x0], #16
	mrs x1, cpacr_el1
	mrs x2, contextidr_el1
	stp x1, x2, [x0], #16
	mrs x1, csselr_el1
	mrs x2, esr_el1
	stp x1, x2, [x0], #16
	
	mrs x1, hcr_el2
	mrs x2, ttbr0_el1
	stp x1, x2, [x0], #16
	mrs x1, ttbr1_el1
	mrs x2, vbar_el1
	stp x1, x2, [x0],  #16
	mrs x1, tcr_el1
	mrs x2, mair_el1
	stp x1, x2, [x0], #16
	mrs x1, amair_el1
	mrs x2, tpidr_el0
	stp x1, x2, [x0], #16
	mrs x1, tpidr_el1
	mrs x2, tpidrro_el0
	stp x1, x2, [x0], #16
	mrs x1, vmpidr_el2
	mrs x2,  far_el1
	stp x1, x2, [x0], #16
	mrs x1, par_el1
	mrs x2, afsr0_el1
	stp x1, x2, [x0], #16
	mrs x1, afsr1_el1
	mrs x2, fpsr
	stp x1, x2, [x0], #16
	mrs x1, fpcr
	str x1, [x0], #8

	mrs x2, sp_el0
	mrs x1, sp_el1
	stp x1, x2, [x0], #16

	mrs x1, elr_el1
	mrs x2, spsr_el1
	stp x1, x2, [x0], #16

	/*  return to caller  */
	ret

/* void hv_arch_vcpu_context_restore(struct arch_vcpu *vcpu) */
.global hv_arch_vcpu_context_restore
.type hv_arch_vcpu_context_restore @function
hv_arch_vcpu_context_restore:
	ldp x1, x2, [x0], #16
	msr sctlr_el1, x1
	msr actlr_el1, x2
	ldp x1, x2, [x0], #16
	msr cpacr_el1, x1
	msr contextidr_el1, x2
	ldp x1, x2, [x0], #16
	msr csselr_el1, x1
	msr esr_el1, x2

	ldp x1, x2, [x0], #16
	msr hcr_el2, x1
	msr ttbr0_el1, x2
	ldp x1, x2, [x0], #16
	msr ttbr1_el1, x1
	msr vbar_el1, x2
	ldp x1, x2, [x0], #16
	msr tcr_el1, x1
	msr mair_el1, x2
	ldp x1, x2, [x0], #16
	msr amair_el1, x1
	msr tpidr_el0, x2
	ldp x1, x2, [x0], #16
	msr tpidr_el1, x1
	msr tpidrro_el0, x2
	ldp x1, x2, [x0], #16
	msr vmpidr_el2, x1
	msr far_el1, x2
	ldp x1, x2, [x0], #16
	msr par_el1, x1
	msr afsr0_el1, x2
	ldp x1, x2, [x0], #16
	msr afsr1_el1, x1
	msr fpsr, x2
	ldp x1, x2, [x0], #16
	msr fpcr, x1
	msr sp_el0, x2              
	ldp x1, x2, [x0], #16
	msr sp_el1, x1
	msr elr_el1, x2
	ldr x1, [x0], #8
	msr spsr_el1, x1
	ret

/* void hv_arch_vcpu_launch(void entry, void *arg)  
*  sctl_el1 
   spsr_el2
   el_el2
   sp_el2 
*/
.global hv_arch_vcpu_launch
.type hv_arch_vcpu_launch @function
hv_arch_vcpu_launch:
	ldr x2, =GUEST_SCTLR_EL1
	msr sctlr_el1, x2

	ldr x2, =SPSR_EL2_DEFAULT 
	msr spsr_el2, x2 
	
	msr elr_el2, x0
	mov x0, x1

	mrs x2, midr_el1
	msr vpidr_el2, x2
	//mrs x2, mpidr_el1
	//msr vmpidr_el2, x2

	mov x1, xzr
	mov x2, xzr

	clrex	
	eret
