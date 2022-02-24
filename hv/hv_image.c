/* vm image managment 
 *
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

#include <hv_vm.h>
#include <stdlib.h>
#include <lk/err.h>
#include <lk/trace.h>
#include <stdio.h>
#include <string.h>
#include <lib/elf.h>
#include <lib/fs.h>
#include <hv_image.h>
#include <arch/ops.h>

/* TODO load fit image  */
int vm_load_fit(struct lk_vm *vm)
{
	return 0;
}

static int load_pl(struct lk_vm *vm, filehandle *im_hdl, pheader *pl_hdr)
{
	ssize_t ret;
	paddr_t ld_paddr; 
	void *kvaddr;
	ssize_t f_size, m_size; 
	ssize_t m_zero = 0;

	/* need not load, ignore other types */
	if (pl_hdr->p_type != PT_LOAD)
		return 0;

	f_size = pl_hdr->p_filesz;
	m_size = pl_hdr->p_memsz;

	ld_paddr = ipa_to_pa(&(vm->mem.arch_mem), pl_hdr->p_paddr);
	if (0 == ld_paddr) {
		TRACEF("failed to get physical address for 0x%llx\n", pl_hdr->p_paddr);
		return ERR_NOT_FOUND;
	}

	kvaddr = paddr_to_kvaddr(ld_paddr);
	ret = fs_read_file(im_hdl, kvaddr, pl_hdr->p_offset, f_size);
	if (ret != f_size) {
		TRACEF("failed to read, ret 0x%lx\n", ret);
		return ERR_IO;
	}

	m_zero = m_size - f_size;
	if (0 < m_zero) 
		memset(kvaddr + f_size, 0, m_zero);
	arch_sync_cache_range((addr_t)kvaddr, m_size);

	return 0;
}

/* load elf image */
/* take case the byte of elf. elf is littler endien,
 * arm may be little or may be big, default is little
 * they should be same, checked by verify_eheader  */
int vm_load_elf(struct lk_vm *vm)
{
	ssize_t ret = 0;
	int phdr_num = 0, phdr_len = 0;
	int i = 0;
	char im_file[HV_BUFFER_MIN_LEN * 2] = { 0 }; 
	filehandle *im_handle;
	eheader elf_hdr;
	pheader *pl_hdr;

	snprintf(im_file, sizeof(im_file) - 1, "/%s", vm->info->image);

	ret = fs_open_file(im_file, &im_handle);
	if (ret != NO_ERROR) {
		TRACEF("failed to open file %s\n", im_file);
		return ret;
	}

	ret = fs_read_file(im_handle, &elf_hdr, 0, sizeof(elf_hdr)); 
	if (ret != sizeof(elf_hdr)) {
		TRACEF("failed to open file %s\n", im_file);
		ret = ERR_IO;
		goto failed;
	}

	ret = verify_eheader(&elf_hdr);
	if (ret != NO_ERROR) {
		TRACEF("elf header check failed %d\n", (int)ret);
		goto failed;
	}

	phdr_num = elf_hdr.e_phnum;
	phdr_len = phdr_num * sizeof(*pl_hdr);
	pl_hdr = malloc(phdr_len);
	if (NULL == pl_hdr) {
		TRACEF("failed to allocate memory\n");
		goto failed;
	}

	memset(pl_hdr, 0, phdr_len);
	ret = fs_read_file(im_handle, pl_hdr, elf_hdr.e_phoff, phdr_len); 
	if (ret != phdr_len) {
		TRACEF("failed to read programmer header\n");
		ret = ERR_IO;
		goto p_failed;
	}

	for (i = 0; i < phdr_num; i++) {
		ret = load_pl(vm, im_handle, pl_hdr + i);
		if (ret != NO_ERROR) {
			TRACEF("failed to read program segment %d ret %d\n", i, (int)ret);
			ret = ERR_IO;
			goto p_failed;
		}
	}
	ret = 0;
	vm->info->entry = elf_hdr.e_entry;  

p_failed:
	free(pl_hdr);
failed:
	fs_close_file(im_handle);
	return ret;
}

static int calc_fdt_pos(struct lk_vm *vm)
{
	int index = 0;
	struct phy_mem_seg *seg;
	
	seg = vm->mem.mem_seg + index;
	if ((seg->ipa <= vm->info->entry) && 
			(vm->info->entry <= seg->ipa + VM_MEM_BLOCK_2M))
		index = vm->mem.seg_num - 1;
	return index;
}

/* load fdt into vm  */
int vm_load_fdt(struct lk_vm *vm)
{
	ssize_t ret = 0;
	int index = 0;
	char dtb_file[HV_BUFFER_MIN_LEN * 2] = { 0 };
	filehandle *dtb_handle = NULL;
	paddr_t ld_addr;
	void *kva;
	struct file_stat stat;

	/* no dtb to load, then return */
	if (0 == strlen(vm->info->fdt))
		return 0;

	snprintf(dtb_file, sizeof(dtb_file) - 1 , "/%s", vm->info->fdt);
	ret = fs_open_file(dtb_file, &dtb_handle);
	if (ret != NO_ERROR) {
		TRACEF("failed to open file %s\n", dtb_file);
		return ret;
	}	

	/* load into the first ram segment */
	index = calc_fdt_pos(vm);
	ld_addr = vm->mem.mem_seg[index].pa;
	kva = paddr_to_kvaddr(ld_addr);

	ret = fs_stat_file(dtb_handle, &stat);
	if (ret != NO_ERROR) {
		TRACEF("failed to stat file %s\n", dtb_file);
		goto failed;
	}

	ret = fs_read_file(dtb_handle, kva, 0, stat.size);
	if  (ret != (ssize_t)stat.size) {
		ret = ERR_IO;
		TRACEF("failed to read file, ret 0x%lx\n", ret);
		goto failed;
	}

	ret = 0;
	arch_sync_cache_range((addr_t)kva, stat.size);
	vm->info->args = vm->mem.mem_seg[index].ipa;

failed:
	fs_close_file(dtb_handle);	
	return ret;
}
