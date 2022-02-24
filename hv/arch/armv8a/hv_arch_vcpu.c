/* hv vcpu arch related
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

#include <lk/err.h>
#include <stdio.h>
#include <kernel/vm.h>
#include <sys/types.h>
#include <arch/arm64.h>
#include <string.h>
#include <hv_arch_vcpu.h>
#include <hv_vcpu.h>
#include <hv_vm.h>
#include <hv_vgic.h>
#include <hv_vtimer.h>

/* switch to new vcpu */
void hv_vcpu_arch_contex_switch(struct lk_vcpu *new_vcpu, struct lk_vcpu *old_vcpu)
{
	struct lk_vm *vm;

	if (old_vcpu) {
		hv_arch_vcpu_context_save(&(old_vcpu->arch_vcpu));
		
		/* interrupt context save   */
		hv_vcpu_irq_save(old_vcpu);
		
		/* timer context save  */
		hv_vtimer_context_save(old_vcpu);
	} 

	/* stage 2 table init  */
	vm = new_vcpu->vm;
	ARM64_WRITE_SYSREG(vttbr_el2, vm->mem.arch_mem.vttbr);

	hv_arch_vcpu_context_restore(&(new_vcpu->arch_vcpu));
	
	/* interrupt context restore  */
	hv_vcpu_irq_restore(new_vcpu);

	/* timer context restore  */
	hv_vtimer_context_restore(new_vcpu);
	return ;
}

/* mpidr to cpu id */
/* for virtual mpidr, only support aff0 an aff1. 
 * aff0 is only 4 bits, for the reason of GICV3 SGI.
 * so the max vcpu id is 1 << (4 + 8) - 1  */
uint32_t mpidr_to_cpu_id(uint64_t mpidr)
{
	uint32_t cpu_id;
	
	cpu_id = mpidr & MPIDR_LEVEL_MASK;
	cpu_id = cpu_id & ((1 << MPIDR_MAX_AFF0_BITS) - 1);
	cpu_id |= ((mpidr >> MPIDR_LEVEL_SHIFT(1)) & MPIDR_LEVEL_MASK) << MPIDR_MAX_AFF0_BITS; 
	
	return cpu_id;
}

/* cpuid to mpidr  */
/* format of mpidr : U/MT is 0  */
uint64_t cpu_id_to_mpidr(uint32_t cpu_id)
{
	uint64_t mpidr = 0;
	
	mpidr = cpu_id & ((1 << MPIDR_MAX_AFF0_BITS) - 1);
	mpidr |= ((cpu_id >> MPIDR_MAX_AFF0_BITS) & MPIDR_LEVEL_MASK) << 8;
		 
	return mpidr;
}

/* vcpu  arch init  */
/* hcr_el2 */
/* hstr_el2 : Hypervisor System Trap Register */
/* CPTR_EL2 : Architectural Feature Trap Register (EL2) */
/* CNTHCTL_EL2 0  */
/* CPACR_EL1: Architectural Feature Access Control Register
 * Controls access to trace, SVE, and Advanced SIMD and floating-point functionality. */
/* currently does not support aarch32 mode   */
void hv_vcpu_arch_init(struct arch_vcpu *vcpu)
{
	struct lk_vcpu *o_vcpu = NULL;

	o_vcpu = container_of(vcpu, struct lk_vcpu, arch_vcpu);

	vcpu->hcr_el2 = HCR_GUEST_DEFAULT;
	vcpu->vmpidr_el1 = cpu_id_to_mpidr(o_vcpu->id);
       	ARM64_WRITE_SYSREG(hstr_el2, HSTR_DEFAULT_VALUE);
	ARM64_WRITE_SYSREG(cptr_el2, CPTR_DEFAULT_VALUE); 
	ARM64_WRITE_SYSREG(cnthctl_el2, 0);                  // TODO timer
	ARM64_WRITE_SYSREG(cpacr_el1, 0);
	return ;
}
