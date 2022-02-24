/*  hv memory arch related functions 
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
#include <arch/arm64/mmu.h>
#include <arch/arm64.h>
#include <kernel/vm.h>
#include <hv_mem.h>
#include <string.h>
#include <arch/atomic.h>
#include <lk/trace.h>

#define IS_PAGE_ALIGN(len) !((len) & (!(PAGE_SIZE -1)))

/* using 16 bit as vmid 
 * using a simple method to calc vmid 
 * TODO: improve vmid: 
 * 1. trace which one is not used  
 * 2. lock protect 
 * 3. must maller than vmid_bits */
static volatile int base_vmid = 1;

/* default is 8 bits  */
static uint32_t vmid_bits = 8;
static uint32_t root_order = -1;
static uint32_t root_level = -1;

uint32_t is_root_level(uint32_t level)
{
	return root_level == level;
}

static uint16_t get_vmid(void)
{
	return atomic_add(&base_vmid, 1);	
}

/* init root directory  */
static int init_st2_root(struct arch_vm_mem *arch_mem)
{
	size_t ret = NO_ERROR;
	paddr_t st2_pa;
	struct list_node *ln = NULL;

	ln = &(arch_mem->ln);
	list_initialize(ln);
	ret = pmm_alloc_contiguous(1 << root_order, PAGE_SIZE_SHIFT + root_order, 
		&st2_pa, ln);			
	if (ret != (size_t)(1 << root_order)) {
		TRACEF("failed to allocate memory for root\n");
		return ERR_NO_MEMORY;
	}

	ret = 0;
	arch_mem->st2_table = st2_pa;
	if ((unsigned long)-1 == arch_mem->st2_table) {
		TRACEF("failed to get paddr for st2 root page\n");
		ret = ERR_NO_MEMORY;
		goto failed;
	}

	arch_mem->st2_root_kva = paddr_to_kvaddr(arch_mem->st2_table);
	if (arch_mem->st2_root_kva == NULL) {
		TRACEF("failed to get vaddr for kaddr 0x%lx\n", arch_mem->st2_table);
		ret = ERR_NOT_FOUND;
		goto failed;
	}

	arch_mem->vmid = get_vmid();	
	arch_mem->vttbr = ((uint64_t)arch_mem->vmid << (64 - VTCR_EL2_VMID_BITS))|arch_mem->st2_table;
	memset(arch_mem->st2_root_kva, 0, PAGE_SIZE);
	return ret;	

failed:
	pmm_free(ln);
	return ret;
}

/*  get @level pte index of @ipa   */
static uint32_t pte_index(paddr_t ipa, int level)
{
	uint32_t index = 0;
	int off_shift = 0;
	int bit_shift = VM_MEM_PTE_SHIFT * (3 - level) + PAGE_SIZE_SHIFT;
	
	if ((int)root_level == level)
		off_shift = root_order;
	index = ipa >> bit_shift;
	index = index & (( 1 << (VM_MEM_PTE_SHIFT + off_shift)) -1);

       	return index;	
}

/* level 0 1 2 3 is valid pte  */
static int is_pte_valid(pte_t pte)
{
	return (pte & (uint64_t)0x1);
}

/* pte 3 is reserved : the last 2 bit is 0 1 */
static int is_pte_res(pte_t pte)
{
	return (pte & (uint64_t)0x1) && (!((pte >> 1) & (uint64_t)0x1));
}

/* level 0 1 2 is block: the last 2 bit is 0 1 */
static int is_pte_block(pte_t pte)
{
	return (pte & (uint64_t)0x1) && (!((pte >> 1) & (uint64_t)0x1));
}

/* get the physical address based on pte, 
 * caller should check that level must  not be 0 */
static paddr_t pte_block_address(pte_t pte, int level)
{
	paddr_t block_size[4] = {-1, VM_MEM_BLOCK_1G, VM_MEM_BLOCK_2M, PAGE_SIZE};
	return ((pte) & (MMU_OA_RANGE_MASK)) & (~(block_size[level] - 1));
}

/* get the page table address pointer by this pte */
static paddr_t pte_table_address(pte_t pte)
{
	return ((pte) & (MMU_OA_RANGE_MASK)) & (~(PAGE_SIZE - 1));
}

/* get the pte at the addr @ipa level is @level */
static pte_t *pte_get(struct arch_vm_mem *arch_mem, paddr_t ipa, int level)
{
	int i  = 0;
	pte_t *st2_kva = NULL;
	paddr_t st2_table = arch_mem->st2_table;
	uint32_t index = 0;
	pte_t *pte;

	if (st2_table == 0) {
		if (init_st2_root(arch_mem) != NO_ERROR) {
			TRACEF("failed to init st2 root\n");
			return NULL;
		}
		st2_table = arch_mem->st2_table;

	}

	st2_kva = arch_mem->st2_root_kva;
	for (i = root_level; i < level; i++) {

		index = pte_index(ipa, i);
		pte = st2_kva + index;
		if (*pte == 0 || !is_pte_valid(*pte)) {
			vm_page_t *pg = NULL;
			paddr_t phaddr;
			pte_t val;
			
			pg = pmm_alloc_page();
			if (NULL == pg) {
				TRACEF("failed to allocate memory for ipa %lx level %u\n", ipa, i);
				return NULL;
			}

			phaddr = vm_page_to_paddr(pg);
			if (phaddr == (unsigned long)-1) {
				TRACEF("failed to get physical address for page\n");
				return NULL;
			}
			
			st2_kva = paddr_to_kvaddr(phaddr);
			memset(st2_kva, 0, PAGE_SIZE);	
			val = MMU_PTE_TABLE|phaddr;
			*pte = val;
		} else {
			/* check whether this pte is final  
			 * level 3 or block is final, there is not next level one 
			 * i == 3 will be always false, for the biggest level is 3 */
			if (i == 3 || is_pte_block(*pte))
				return NULL;

			/* get the entry  */
			paddr_t phaddr = pte_table_address(*pte);
			st2_kva = paddr_to_kvaddr(phaddr);
		}	
	}

	/* get the last level entry  */
	index = pte_index(ipa, i);
	pte = st2_kva + index;
	
	return pte;
}

/* tlb flush: 
 * should check whether mem is the current one  */
void arch_tlb_flush(struct vm_mem *mem)
{
	uint64_t vttbr = 0;
	struct arch_vm_mem *arch_mem;

	arch_mem = &(mem->arch_mem);
	vttbr = ARM64_READ_SYSREG(vttbr_el2);
	if (vttbr != arch_mem->vttbr)
		ARM64_WRITE_SYSREG(vttbr_el2, arch_mem->vttbr);
	
	tlb_flush();

	if (vttbr != arch_mem->vttbr)
		ARM64_WRITE_SYSREG(vttbr_el2, vttbr);
	
	return;
}

/* set pte value to   */
static void set_pte(pte_t *pte, pte_t val)
{
	asm volatile (
		"dsb sy;"
		"str %1, [%0];"
		"dsb sy;"
		::"r"(pte), "r"(val) : "memory");
}

/* Permission mask: xn, write, read */
#define ST2_PERM_MASK (0x00400000000000C0ULL)
#define ST2_PERM_NOT(pte) (pte & ~ST2_PERM_MASK)

/*  set pte @dst to @val 
 *  use the break-before-make rule when not init 
 *  @ipa is the address for pte @dst 
 *  currently yes or no. 
 *  tlb flush method:
 *  1. if just remove a valid map , defer it 
 *  2. if permission changed , defer it 
 *  3. if original pte is not valid, does not need flush 
 *  4. others cases, flush at once
 */
static void mem_pte_set(struct vm_mem *mem, pte_t *dst, pte_t val, paddr_t ipa)
{
	pte_t orig_dst;

	/* original is not valid, set new value, and return  */
	if (!is_pte_valid(*dst)) {
		set_pte(dst, val);
		return;
	}

	/* if update, should follow  break-before-make rule: D5.10.1, 
	 * which list the  cases to follow that rules, 
	 * change permission is not in that list   */
	
	/* update to invalid value first */
	orig_dst = *dst;
	set_pte(dst, 0);

	/* set an invalid value, return, such as just unmap a table */
	if (!is_pte_valid(val)) {
		mem->need_flush = 1;       /* defered */
		return;
	} else {

		if (ST2_PERM_NOT(orig_dst) != ST2_PERM_NOT(val)) 
			tlb_flush_ipa(ipa);
		else
			mem->need_flush = 1;

		/* val is valid */
		set_pte(dst, val);
	}

	return;
}

/* map @ipa to @pa, memory block is @len, attribute is @flags 
 * stage 2 page table map it is format is like this :
 * locker should take mem lock */
int arch_mem_map(struct vm_mem *vm_mem, paddr_t pa, paddr_t ipa, 
		ssize_t len, int flags)
{
	int ret = 0;
	struct arch_vm_mem *arch_mem;

	arch_mem = &(vm_mem->arch_mem);
	/* IA & OA should be smaller than 48 bits  */
	if (pa & (~MMU_OA_RANGE_MASK) || 
			ipa & (~MMU_OA_RANGE_MASK)) {
		TRACEF("pa/ipa is wrong %lx %lx\n", pa, ipa);
		return ERR_INVALID_ARGS;
	}

	/* it should be PAGE_SIZE align pa/ipa/len */
	if (!IS_PAGE_ALIGN(len) ||
			len < (int)PAGE_SIZE ||
			!IS_PAGE_ALIGN(pa) ||
			!IS_PAGE_ALIGN(ipa)) {
		TRACEF("pa/ipa is page align %lx %lx\n", pa, ipa);
		return ERR_INVALID_ARGS;
	}

	while(len > 0) {

		/* this is a 2M block  */
		if (!(pa & VM_MEM_BLOCK_MASK) && 
				!(ipa & VM_MEM_BLOCK_MASK) &&
				!(len & VM_MEM_BLOCK_MASK)) {
			pte_t src;
			pte_t *dst;

			dst = pte_get(arch_mem, ipa, 2);
			if (NULL == dst) {
				ret = ERR_NO_MEMORY;
				goto out;
			}

			src = MMU_PTE_BLOCK|pa;
			mem_pte_set(vm_mem, dst, src, ipa);
			len = len - VM_MEM_BLOCK_2M;
			pa = pa + VM_MEM_BLOCK_2M;
			ipa = ipa + VM_MEM_BLOCK_2M;

		} else {
			/* this is PAGE_SIZE align blocks */
			pte_t src;
			pte_t *dst;

			dst = pte_get(arch_mem, ipa, 3);
			if (NULL == dst) {
				ret = ERR_NO_MEMORY;
				goto out;
			}	

			src = MMU_PTE_TABLE|pa;
			mem_pte_set(vm_mem, dst, src, ipa);
			len = len - PAGE_SIZE;
			pa = pa + PAGE_SIZE;
			ipa = ipa + PAGE_SIZE;
		}
	}

out:
	return ret;
}

/* unmap a stage2 segment ipa is @ipa, len is @len  
 * lock protect 
 * TODO : does it need cache operations  */
int arch_mem_unmap(struct vm_mem *vm_mem, paddr_t ipa, ssize_t len)
{
	int ret = 0;
	struct arch_vm_mem *arch_mem;

	arch_mem = &(vm_mem->arch_mem);
	/* ipa should be smaller than 48 bits  */
	if (ipa & (~MMU_OA_RANGE_MASK)) {
		TRACEF("pa/ipa is wrong %lx\n", ipa);
		return ERR_INVALID_ARGS;
	}

	/* it should be PAGE_SIZE alignipa/len */
	if (!IS_PAGE_ALIGN(len) ||
			len < (int)PAGE_SIZE ||
			!IS_PAGE_ALIGN(ipa)) {
		TRACEF("pa/ipa is page align %lx\n", ipa);
		return ERR_INVALID_ARGS;
	}

	while(len > 0) {

		/* this is a 2M block  */
		if (!(ipa & VM_MEM_BLOCK_MASK) &&
				!(len & VM_MEM_BLOCK_MASK)) {
			pte_t *dst;

			dst = pte_get(arch_mem, ipa, 2);
			if (NULL == dst) {
				ret = ERR_NOT_FOUND;
				goto out;
			}

			mem_pte_set(vm_mem, dst, 0, ipa);
			len = len - VM_MEM_BLOCK_2M;
			ipa = ipa + VM_MEM_BLOCK_2M;
		} else {
			/* this is PAGE_SIZE align blocks */
			pte_t *dst;

			dst = pte_get(arch_mem, ipa, 3);
			if (NULL == dst) {
				ret = ERR_NOT_FOUND;
				goto out;
			}	

			mem_pte_set(vm_mem, dst, 0, ipa);
			len = len - PAGE_SIZE;
			ipa = ipa + PAGE_SIZE;
		}
	}

out:
	return ret;
}

/* VTCR_EL2 init during boot */
void arch_mem_init(void)
{
	uint64_t val;
	uint64_t tmp, tmp1;
	uint32_t t0sz;
	uint32_t sl0;

	/* configuration for VTCR_EL2, which is :
	 * IA: please refer  D5-6 , and Supported IPA size 
	 * inter cache/outer cache : write back/read allocate/write allocate
	 * physical size:48 bit
	 * vmid:16 bits */

	/* this information is based on page size is 4k, aarch64 mode */
	struct ipa_info {
		uint32_t t0sz;
		uint32_t sl0;
		uint32_t root_order;
		uint32_t root_level;
	} ipa_info [] = {
		{32, 1, 0, 1},
		{28, 1, 0, 1},
		{24, 1, 1, 1},
		{22, 1, 3, 1},
		{20, 2, 0, 0},
		{16, 2, 0, 0},
	};

	/* get pasize from ID_AA64MMFR0_EL1 */
	tmp = ARM64_READ_SYSREG(ID_AA64MMFR0_EL1);
	tmp = tmp & (uint64_t)0xf;
	t0sz = ipa_info[tmp].t0sz;
	sl0 = ipa_info[tmp].sl0;

	/* vtcr need to init in each cpu ,
	 * but root_order & root level just need to init once  */
	if (root_order == (uint32_t)-1) {
		root_order = ipa_info[tmp].root_order;
		root_level = ipa_info[tmp].root_level;
	}

	tmp1 = ARM64_READ_SYSREG(ID_AA64MMFR1_EL1);
	tmp1 = tmp1 >> 4;
	tmp1 = tmp1 & 0xf;
	if (tmp1 == 0x2)
		vmid_bits = 16;

	val = t0sz|VTCR_EL2_SL0(sl0)|VTCR_EL2_IRGN0|
		VTCR_EL2_ORGN0|VTCR_EL2_SH0|VTCR_EL2_TG0|
		VTCR_EL2_PS(tmp)|VTCR_EL2_VS(vmid_bits == 16)|
		VTCR_EL2_RES1(31);
	ARM64_WRITE_SYSREG(vtcr_el2 ,(val));
			
	return;
}

/* ipa to pa  */
paddr_t ipa_to_pa(struct arch_vm_mem *arch_mem, paddr_t ipa)
{
	int level;
	pte_t *st2_kva = NULL;
	pte_t *pte = NULL;
	int bit_shift = 0;
	paddr_t phaddr = 0;

	if (ipa & (~MMU_OA_RANGE_MASK)) {
		TRACEF("ipa is wrong 0x%lx\n", ipa);
		return ERR_INVALID_ARGS;
	}

	st2_kva = arch_mem->st2_root_kva;
	if (NULL == st2_kva)
		return 0;

	for (level = root_level; level < 3; level++) {
		uint32_t index = 0;

		index = pte_index(ipa, level);
		pte = st2_kva + index;
		if (*pte == 0 || !is_pte_valid(*pte))
			return 0;
		else if (is_pte_block(*pte)) {
			
			bit_shift = (3 - level) * VM_MEM_PTE_SHIFT + PAGE_SIZE_SHIFT;
			return pte_block_address(*pte, level) + (ipa & (((paddr_t)1 << bit_shift) - 1));
		} else {
			/* get the entry  */
			phaddr = pte_table_address(*pte);
			st2_kva = paddr_to_kvaddr(phaddr);
		}		
	}

	if (pte == NULL || 
		!is_pte_valid(*pte) ||
	       	is_pte_res(*pte))
		return 0;

	bit_shift = (3 - level) * VM_MEM_PTE_SHIFT + PAGE_SIZE_SHIFT;
	return pte_block_address(*pte, level) + (ipa & (((paddr_t)1 << bit_shift) - 1));
}

/* TODO  */
uint64_t va_to_ipa(uint64_t va)
{
	return 0;
}

/* TODO   */
uint64_t va_to_pa(uint64_t va)
{
	return 0;
}
