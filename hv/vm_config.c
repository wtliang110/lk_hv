/* parse vm configuration
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

#include <stdio.h>
#include <libfdt.h>
#include <lk/err.h>
#include <lib/fs.h>
#include <lk/trace.h>
#include <hv_vm.h>
#include <kernel/thread.h>

/* rootdevice parse   */
char *get_root_config(int *cfg_len)
{
	int offset = 0;
	const char *ba_path = "/chosen";
	const char *ba_prop = "bootargs";
	void *boot_arg = NULL;
	void *fdt = (void*)KERNEL_BASE;

	offset = fdt_path_offset(fdt, ba_path);
	if ( 0 > offset) {
		TRACEF("failed to find %s return %d\n", ba_path, offset);
		return NULL;	
	}

	boot_arg =  (char *)fdt_getprop(fdt, offset, ba_prop, cfg_len);
        if (NULL == boot_arg) {
		TRACEF("failed to find %s at offset %d", ba_prop, offset);
		return NULL;
	}

	return boot_arg;		
}

/* get cmcfg dtb from file  */
static void *get_vmcfg_dtb(char *vmcfg_path)
{
	ssize_t ret;
	filehandle *cfg_file = NULL;
	void *cfg_dtb = NULL;
	struct file_stat stat; 

	
	ret = fs_open_file(vmcfg_path, &cfg_file);
	if (ret != NO_ERROR) {
		TRACEF("failed to open file\n");
		return NULL;
	}

	ret = fs_stat_file(cfg_file, &stat);
	if (ret != NO_ERROR) {
		TRACEF("file stat failed\n");
		goto st_failed;
	}
	cfg_dtb = malloc(stat.size);
	ret = fs_read_file(cfg_file, cfg_dtb, 0, stat.size);
	if (ret != (ssize_t)stat.size) {
		TRACEF("failed to read file %ld %lld\n", ret, stat.size);
		goto rd_failed;
	}
	fs_close_file(cfg_file);
	return cfg_dtb;

rd_failed:
	free(cfg_dtb);

st_failed:
	fs_close_file(cfg_file);
	return NULL;
}

/* free resource of vmcfg  */
static void put_vmcfg_dtb(void *vmcfg)
{
	free(vmcfg);	
}

static int address_cell = 0;
static int cell_size = 0;

int fdt_get_int_value(void *fdt, int offset, char *prop, int *val)
{
	int len;
	int *ret;

	ret = (int *)fdt_getprop(fdt, offset, prop, &len);
	if (NULL == ret) {
		TRACEF(" failed to get prop %s\n", prop);
		return -1;
	}

	*val = fdt32_to_cpu(*ret);
	return 0;
}

int fdt_get_u64_value(void *fdt, int offset, char *prop, u64 *val)
{
	int len;
	int *ret;

	ret = (int *)fdt_getprop(fdt, offset, prop, &len);
	if (NULL == val) {
		TRACEF(" failed to get prop %s\n", prop);
		return -1;
	}

	*val = fdt64_to_cpu(*ret);
	return 0;
}

static char *fdt_get_str(void *fdt, int offset, char *prop)
{
	int len;
	void *ret;

	ret = (void *)fdt_getprop(fdt, offset, prop, &len);
	if (NULL == ret) {
		TRACEF(" failed to get prop %s\n", prop);
		return NULL;	
	}

	return ret;
}

static inline u64 fdt_read_number(const fdt32_t *cell, int size)
{
	u64 r = 0;
	for (; size--; cell++)
		r = (r << 32) | fdt32_to_cpu(*cell);
	return r;
}


u64 fdt_next_cell(int s, fdt32_t **cellp)
{
	fdt32_t *p = *cellp;

	*cellp = p + s;
	return fdt_read_number(p, s);
}

int get_node_offset(void *dtb, char *path, char *nd_name)
{
	int offset = 0;
	char pa[128] = {0};

	snprintf(pa, sizeof(pa) - 1, "%s/%s", path, nd_name);
	offset = fdt_path_offset(dtb, pa);
	return offset;
}

static int parse_vcpu_cfg(void *dtb, struct vm_info *info, int offset)
{
	int val;
	int ret, len, i;
	void *ptr;

	ret = fdt_get_int_value(dtb, offset, (char *)"cpu_num", &val);
	if (0 > ret) {
		TRACEF("failed to get cpu num\n");
		return ret;
	}

	info->vcpu_num = val;

	ret = fdt_get_int_value(dtb, offset, (char *)"mode", &val);
	if (0 > ret) {
		TRACEF("failed to get cpu num\n");
		return ret;
	}
	info->vcpu_mode = val;

	info->vcpu_affinity = malloc(sizeof(*(info->vcpu_affinity)) * info->vcpu_num);
	if (NULL == info->vcpu_affinity){
		TRACEF("failed to allocate memory for vcpu affinity mask \n");
		return ERR_NO_MEMORY;
	}
	memset(info->vcpu_affinity, 0, sizeof(*(info->vcpu_affinity)) * info->vcpu_num); 

	ptr = (void *)fdt_getprop(dtb, offset, "affinity", &len); 
	if (NULL == ptr) {
		TRACEF(" failed to get affinity\n");
		return -1;
	}
	
	if (len != (int)(sizeof(*(info->vcpu_affinity)) * info->vcpu_num)) {
		TRACEF(" affinity size is wrong %d\n", len);
		return -1;
	}

	for (i = 0; i < info->vcpu_num; i++) {
		val = (int)fdt_next_cell(1, (fdt32_t **)&ptr);
		info->vcpu_affinity[i] = val;
	}

	/*   */
		
	return 0;
}

/* parse @i vm configurationo from dtb file @dtb  */
static int parse_vm_cfg(void *dtb, int i, struct lk_vm *vm)
{
	int offset, child_offset;
	char path[64] = {0};
	char *str = NULL;
	int val = 0, len = 0;
	void *ptr = NULL;
	//u64 val64 = 0;
	struct vm_info *info = vm->info;

	snprintf(path, sizeof(path) - 1, "/vm@%d", i + 1);
	offset = fdt_path_offset(dtb, path);
	if (0 > offset) {
		TRACEF(" failed to get node on path %s\n", path);
		return -1;
	}

	str = (char *)fdt_get_str(dtb, offset, (char *)"image");
	if (str == NULL)
		return -1;
	strncpy(info->image, str, sizeof(info->image) - 1);
	
	str = fdt_get_str(dtb, offset, (char *)"fdt");
	if (NULL != str)
		strncpy(info->fdt, str, sizeof(info->fdt) - 1);

	str = fdt_get_str(dtb, offset, (char *)"lable");
	if (str == NULL)
		return -1;
	strncpy(info->name, str, sizeof(info->name) - 1);

	if (0 > fdt_get_int_value(dtb, offset, (char *)"os_typs", &val))
		return -1;
	info->os_type = val;

	if (0 > fdt_get_int_value(dtb, offset, (char *)"priority", &val))
		return -1;
	if (val > HIGHEST_PRIORITY) {
		TRACEF("invalid priority %d\n", val);
		return ERR_INVALID_ARGS;
	}
	info->priority = val;
	
	if (0 > fdt_get_int_value(dtb, offset, (char *)"timeslice", &val))
		return -1;
	info->timeslice = val;

	if (0 > fdt_get_int_value(dtb, offset, (char *)"image_type", &val))
		return -1;
	info->image_type = val;

	if (0 > fdt_get_int_value(dtb, offset, (char *)"auto_boot", &val))
		info->auto_boot = 0;
	else 
		info->auto_boot = val;
#if 0
	/* elf : will parse elf file to get the entry
	 * plt : will parse the image to get the entry , so remove all of this  */	
	/* entry */
	if (0 > fdt_get_u64_value(dtb, offset, (char *)"entry", &val64))
		return -1;
	info->entry = val64;

	/* ram  */
	offset = get_node_offset(dtb, path, (char *)"ram");
	if (0 > offset) {
		TRACEF(" failed to get node on path %s\n", "ram");
		return -1;
	}
#endif

	ptr = (void *)fdt_getprop(dtb, offset, "ram_reg", &len); 
	if (NULL == ptr) {
		TRACEF(" failed to get ram reg\n");
		return -1;
	}
	
	if (len < (int)((address_cell + cell_size) * sizeof (int))) {
		TRACEF(" ram reg len is wrong %d\n", len);
		return -1;

	}

	info->ipa_start = fdt_next_cell(address_cell,(fdt32_t **) &ptr);
	info->ram_size = fdt_next_cell(cell_size, (fdt32_t **)&ptr);
	info->ram_size = info->ram_size << 20;    // M ---> byte
	
	/* cpu */
	child_offset = fdt_subnode_offset(dtb, offset, "cpus");
	if (0 > child_offset) {
		TRACEF(" failed to get node cpus offset\n %d", child_offset);
		return -1;
	}
	
	if (0 > parse_vcpu_cfg(dtb, info, child_offset)) {
		TRACEF("failed to parse vcpu info\n");
		return -1;
	}
	
	/* devices */
	info->vm_cfg = dtb;
	info->cfg_offset = offset;

	return 0;
}

/* parse vm configure based on dtb  */
int lkhv_parse_vmcfg(char *vmcfg)
{
	int ret;
	int offset = 0;
	int vm_num = 0; 
	void *vmcfg_dtb = NULL;
	int i;

	vmcfg_dtb = get_vmcfg_dtb(vmcfg);
	if (NULL == vmcfg_dtb) {
		return ERR_NO_MEMORY;
	}

	offset = fdt_path_offset(vmcfg_dtb, "/");
	if (0 > offset) {
		TRACEF("failed to get offset\n");
		ret = -1;
		goto failed;
	}

	address_cell = fdt_address_cells(vmcfg_dtb, offset);
	cell_size = fdt_size_cells(vmcfg_dtb, offset);

	ret = fdt_get_int_value(vmcfg_dtb, offset, (char *)"num-vms", &vm_num);
	if (0 > ret) {
		TRACEF("failed to get vm nums\n");
		ret = -2;
		goto failed;
	}

	for (i = 0; i < vm_num; i++) {
		struct lk_vm *vm = NULL;
	
		vm = lkhv_alloc_vm();
		if (vm == NULL) {
			TRACEF("faield to allocate vm\n");
			break;
		}
			
		ret = parse_vm_cfg(vmcfg_dtb, i, vm);
		if (0 > ret) {
			TRACEF("failed to parese vm %d\n", i);
			break;
		}
		
		ret = lkhv_init_vm(vm);
		if (0 > ret) {
			TRACEF("failed to create vm %s\n", vm->info->name);
			lkhv_free_vm(vm);
			break;
		}	
	}

	return 0;

failed:
	put_vmcfg_dtb(vmcfg_dtb);
	return ret;
}
