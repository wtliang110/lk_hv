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
#ifndef __VM_ARCH_MEM_H__
#define __VM_ARCH_MEM_H__
#include <sys/types.h>
#include <arch/arm64/mmu.h>
#include <kernel/vm.h>

#define VM_MEM_BLOCK_1G_SHIFT 30
#define VM_MEM_BLOCK_1G      (1 << VM_MEM_BLOCK_1G_SHIFT)
#define VM_MEM_BLOCK_SHIFT 21      /*  aligned by 2M  */
#define VM_MEM_BLOCK_2M (1 << 21)
#define VM_MEM_BLOCK_MASK ((1 << VM_MEM_BLOCK_SHIFT) - 1)
#define VM_IA_SIZE_BIT 48    /* 48 bits ipa & pa size  */
#define VM_MEM_PTE_SHIFT 9 

struct vm_mem;
struct arch_vm_mem {
	u64 vttbr;
	paddr_t st2_table;
	struct list_node ln;  /* root list  */
	void *st2_root_kva;
	uint32_t vmid;
};

/* IA range  */
#define MMU_OA_RANGE_BITS 48
#define MMU_OA_RANGE_MASK (((unsigned long)1 << 48) - 1)

/*  VTCR CONTROL BIT   */
#define MMU_VTCR_FLAGS0 MMU_TCR_FLAGS0_1
#define MMU_PTE_S2AP_RW BM(6, 2, 3)
#define MMU_PTE_S2AP_RO BM(6, 2, 1)
#define MMU_PTE_S2AP_WO BM(6, 2, 2)
#define MMU_PTE_S2MA_DEV_NGNRNE BM(2, 4, 0) //0b0000
#define MMU_PTE_S2MA_DEV_NGNRE BM(2, 4, 1)  //0b0001
#define MMU_PTE_S2MA_DEV_NGRE BM(2, 4, 2)  //0b0010
#define MMU_PTE_S2MA_DEV_GRE  BM(2, 4, 3)  //0b0011
#define MMU_PTE_S2MA_MEM_INC  BM(2, 4, 4)  //0b0100
#define MMU_PTE_S2MA_MEM_OWT  BM(2, 4, 8)  //0b1000
#define MMU_PTE_S2MA_MEM_OWB  BM(2, 4, 12) //0b1100

#define MMU_PTE_BLOCK  MMU_PTE_L012_DESCRIPTOR_BLOCK|\
			MMU_PTE_ATTR_AF|\
			MMU_PTE_ATTR_SH_INNER_SHAREABLE|\
			MMU_PTE_S2AP_RW |\
			MMU_PTE_S2MA_MEM_INC
			
#define MMU_PTE_TABLE   MMU_PTE_L012_DESCRIPTOR_TABLE|\
			MMU_PTE_ATTR_AF|\
			MMU_PTE_ATTR_SH_INNER_SHAREABLE|\
			MMU_PTE_S2AP_RW |\
			MMU_PTE_S2MA_MEM_INC

/* VTCR_EL2 CONTROLS */
#define VTCR_EL2_T0SZ   (64 - VM_IA_SIZE_BIT)

#define VTCR_EL2_SL0_L0 0x2
#define VTCR_EL2_SL0_L1 0x1
#define VTCR_EL2_SL0_L2 0x0
#define VTCR_EL2_SL0_L3 0x3
#define VTCR_EL2_SL0(val)    BM(6, 2, (val))

/* cache  */
#define VTCR_EL2_NC    0x0
#define VTCR_EL2_WBWA  0x1
#define VTCR_EL2_WTNWA 0x2
#define VTCR_EL2_WBNWA 0x3
#define VTCR_EL2_IRGN0 BM(8, 2, VTCR_EL2_WBWA)
#define VTCR_EL2_ORGN0 BM(10,2, VTCR_EL2_WBWA)

/* SH  */
#define VTCR_EL2_NSH 0x0
#define VTCR_EL2_OSH 0x2
#define VTCR_EL2_ISH 0x3
#define VTCR_EL2_SH0 BM(12, 2, VTCR_EL2_ISH)

/* TG  */
#define VTCR_EL2_TG MMU_TG0(PAGE_SIZE_SHIFT)
#define VTCR_EL2_TG0 BM(14, 2, VTCR_EL2_TG)

#define VTCR_EL2_VMID_BITS 16
#define VTCR_EL2_VMID  (VTCR_EL2_VMID_BITS == 16)
#define VTCR_EL2_PS(val)  BM(16, 3, (val))          
#define VTCR_EL2_VS(val)  BM(19, 1, val)       //configure vmid bits
#define VTCR_EL2_RES1(val) (uint32_t)(1 << (val))       //configure vmid bits

/* tlb fush operations 
 * all of those operations are applied to current vmid */
static inline void tlb_flush(void)
{
	asm volatile (
		"dsb sy;"
		"tlbi vmalls12e1is;"
		"dsb sy;"
		"isb;"
		: : :"memory");
}

static inline void tlb_flush_local(void)
{
	asm volatile (
		"dsb sy;"
		"tlbi vmalls12e1;"
		"dsb sy;"
		"isb;"
		: : :"memory");
}

static inline void tlb_flush_ipa(paddr_t ipa)
{
	asm volatile (
		"dsb sy;"
		"tlbi ipas2e1is, %0;"
		"dsb sy;"
		"tlbi vmalle1is;"
		"isb;"
		: :"r"(ipa): "memory");
}

int arch_mem_unmap(struct vm_mem *vm_mem, paddr_t ipa, ssize_t len);
int arch_mem_map(struct vm_mem *vm_mem, paddr_t pa, paddr_t ipa, 
		ssize_t len, int flags);
void arch_mem_init(void);
void arch_tlb_flush(struct vm_mem *vm_mem);

paddr_t ipa_to_pa(struct arch_vm_mem *arch_mem, paddr_t ipa);
uint32_t is_root_level(uint32_t level);
#endif 
