/* @author pwang
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

#ifndef __HV_ARCH_VCPU__
#define __HV_ARCH_VCPU__
#include <hv_arch_vcpu_defs.h>
#include <hv_vgic.h>
#include <hv_vtimer.h>

struct arch_vcpu {
	/* registers to save including mmu/cache/exception/pid/system ctl/fp */
	/* current only support 64 bit guest  */
	uint64_t sctlr_el1;
	uint64_t actlr_el1;
	uint64_t cpacr_el1;   /*  Architectural Feature Access Control Register  */
	uint64_t contextidr_el1; /* Context ID Register (EL1),Identifies the current Process Identifier. */
	uint64_t csselr_el1;  /* Cache Size Selection Register */
	uint64_t esr_el1;	
	uint64_t hcr_el2;
	uint64_t ttbr0_el1;
	uint64_t ttbr1_el1;
	uint64_t vbar_el1;
	uint64_t tcr_el1;     /*   */
	uint64_t mair_el1;    /* Memory Attribute Indirection Register  */
	uint64_t amair_el1;   /* Auxiliary Memory Attribute Indirection Register  */
	uint64_t tpidr_el0;   /* EL0 Read/Write Software Thread ID Register */
	uint64_t tpidr_el1;   /*  EL0 Read-Only Software Thread ID Register */
	uint64_t tpidrro_el0; /*   */
	uint64_t vmpidr_el1;  /* Multiprocessor Affinity Register  */
      	uint64_t far_el1;
	uint64_t par;         /*  Physical Address Register  */
	uint64_t afsr0_el1;
	uint64_t afsr1_el1;   /* Auxiliary Fault Status Register 1  */
	
	/* float point  */
	uint64_t fpsr;
	uint64_t fpcr;      /* Floating-point Control Register  */

	/* kernel sp  */
	uint64_t sp_el0;
	uint64_t sp_el1;
	uint64_t elr_el1;
	uint64_t spsr_el1;

	/* gic redistributor */
	struct vgic_redist vgic;
	
	/* timer */
	struct arch_vtimer vtimer;

	/* so on  */
};

void hv_arch_vcpu_context_save(struct arch_vcpu *vcpu); 
void hv_arch_vcpu_context_restore(struct arch_vcpu *vcpu);
uint32_t mpidr_to_cpu_id(uint64_t mpidr);
uint64_t cpu_id_to_mpidr(uint32_t cpu_id);

#define MPIDR_MAX_LEVEL      2
#define MPIDR_MAX_AFF0_BITS  4
#define MPIDR_LEVEL_MASK   ((1 << 8) - 1)
#define MPIDR_LEVEL_SHIFT(level) (level << 3)

#endif 
