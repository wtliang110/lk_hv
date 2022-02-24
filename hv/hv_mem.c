/* hypervisor memory virtulization 
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
#include <hv_vm.h>
#include <hv_mem.h>
#include <stdio.h>
#include <kernel/vm.h>
#include <kernel/spinlock.h>
#include <lk/trace.h>

/* cal memory segment counts */
static int mem_seg_counts(struct vm_info *info)
{
	ssize_t phy_size = 0;

	phy_size = info->ram_size;
	if (phy_size & VM_MEM_BLOCK_MASK) {
		TRACEF("size must be aligned by 2M 0x%lx\n", phy_size);
		return ERR_INVALID_ARGS;
	}

	return phy_size >> VM_MEM_BLOCK_SHIFT;
}

/* allocation physical memory for @vm  */
int lkhv_alloc_mem(struct lk_vm *vm)
{
	int seg_num = 0, i;
	struct vm_mem  *mem;
	unsigned long ipa;
				
	mem = &(vm->mem);

	/* count memory seg mement */
	seg_num = mem_seg_counts(vm->info);
	if (0 > seg_num) {
		return ERR_INVALID_ARGS;
	}

	mem->seg_num = seg_num;
	mem->mem_seg = malloc(sizeof(struct phy_mem_seg) * seg_num);
	if (mem->mem_seg == NULL) {
		TRACEF("failed to allocate memory \n");
		return ERR_NO_MEMORY;
	}

	ipa = vm->info->ipa_start;	
	for (i = 0; i < mem->seg_num; i++) {
		paddr_t pa;
		struct list_node *pgs = NULL;	
		int count = 1 << (VM_MEM_BLOCK_SHIFT - PAGE_SIZE_SHIFT);
		int ret = 0;	
			
		pgs = &(mem->mem_seg[i].pgs);
		list_initialize(pgs);
		ret = pmm_alloc_contiguous(count, VM_MEM_BLOCK_SHIFT, &pa, pgs);
		if (ret != count) {
			TRACEF("failed to allocate memory %d %d\n", ret, i);
			goto failed;
		}
		
		mem->mem_seg[i].ipa = ipa;
		mem->mem_seg[i].pa = pa;
		ipa = ipa + (1 << VM_MEM_BLOCK_SHIFT);
	}

	mem->mem_lock = SPIN_LOCK_INITIAL_VALUE;

	return 0;

failed:
	if (NULL != mem->mem_seg)
		free(mem->mem_seg);

	for (i = 0; i < mem->seg_num; i++) {
		struct phy_mem_seg *seg;

		seg = mem->mem_seg + i;
		if (seg->pa)
			pmm_free(&(seg->pgs));
		else
			break;
	}

	return ERR_NO_MEMORY;
}

/* init memory related controls  */
void lkhv_mem_init(void)
{
	arch_mem_init();
	return;
}

void mem_lock(spin_lock_saved_state_t *state, struct vm_mem *mem)
{
	spin_lock_save(&(mem->mem_lock), state, SPIN_LOCK_FLAG_INTERRUPTS);
}

void mem_unlock(spin_lock_saved_state_t state, struct vm_mem *mem)
{
	/* if needed tlb flush, flust it first */
	if (mem->need_flush) {
		arch_tlb_flush(mem);
		mem->need_flush =  0;
	}

	spin_unlock_restore(&(mem->mem_lock), state, SPIN_LOCK_FLAG_INTERRUPTS);
}

/* unmap physical ram */
void lkhv_unmap_ram(struct vm_mem *mem) 
{
	int i;
	struct phy_mem_seg *mem_seg;
	spin_lock_saved_state_t state;
	ssize_t seg_len = 1 << VM_MEM_BLOCK_SHIFT;
	
	mem_lock(&state, mem);
	for (i = 0; i < mem->seg_num; i++) {
		mem_seg = mem->mem_seg + i;
		arch_mem_unmap(mem, mem_seg->ipa, seg_len);
	}
	mem_unlock(state, mem);

	return;
}

/* map physical ram */
/* TODO flags  */
int lkhv_map_ram(struct lk_vm *vm) 
{
	int i;
	int ret = 0;
	struct vm_mem *mem;
	struct phy_mem_seg *mem_seg;
	ssize_t seg_len = 1 << VM_MEM_BLOCK_SHIFT;
	unsigned long flags = 0;
	spin_lock_saved_state_t state;
		
	mem = &(vm->mem);
	mem_lock(&state, mem);
	for (i = 0; i < mem->seg_num; i++) {
		mem_seg = mem->mem_seg + i;
		ret = arch_mem_map(mem, mem_seg->pa, mem_seg->ipa, seg_len, flags);
		if (ret < 0) {
			mem_unlock(state, mem);
			goto failed;
		}
	}

	mem_unlock(state, mem);
	return 0;

failed:
	lkhv_unmap_ram(mem);
	return ret;
}

/* only for test map a pl011 for ut test  */
int lkhv_map_test_serial(struct lk_vm *vm)
{
	int ret = 0;
	struct vm_mem *mem;
	
	mem = &(vm->mem);
	ret = arch_mem_map(mem, 0x9000000, 0x9000000, 0x1000, 0);
	if (ret < 0)
		TRACEF("failed to map test device %d\n", ret);

	return ret;
}

/* TODO map device memory  */
int lkhv_map_dev_mem(struct lk_vm *vm)
{
	return 0;
}

/* TODO umap device memory */
void lkhv_unmap_dev_mem(struct lk_vm *vm)
{
	return;
}

/* free physical memory of @vm  */
void lkhv_vm_free_mem(struct lk_vm *vm)
{
	int i = 0; 
	struct vm_mem  *mem;

	mem = &(vm->mem);
	for (i = 0; i < mem->seg_num; i++) {
		struct phy_mem_seg *seg;

		seg = mem->mem_seg + i;
		if (seg->pa) 
			pmm_free(&(seg->pgs));
		else
			break;
	}

	free(mem->mem_seg);
	mem->mem_seg = NULL;
}
