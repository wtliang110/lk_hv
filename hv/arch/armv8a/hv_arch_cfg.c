/*  hv configure arch related functions 
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
#include <hv_vm.h>
#include <string.h>
#include <hv_vgic.h>
#include <lk/trace.h>
#include <libfdt.h>
#include <vm_config.h>

/* vm interrupt cfg   */
int hv_vm_irq_cfg_get(struct lk_vm *vm)
{
	int val, len;
	void *ptr;
	int cell_size, address_cell;
	int gic_offset = 0;
	struct vgic *vgic = (struct vgic *)(vm->interrupt);
	struct vgic_cfg *cfg = &(vgic->cfg);
	void *vm_dtb = vm->info->vm_cfg;

	gic_offset = fdt_subnode_offset_namelen(vm_dtb, 
						vm->info->cfg_offset,
						"intc", 4);
	if (gic_offset < 0) {
		TRACEF("failed to find node intc %d %d\n", 
				gic_offset, vm->info->cfg_offset);
		return ERR_NOT_FOUND;
	}
	
	address_cell = fdt_address_cells(vm_dtb, gic_offset);
        cell_size = fdt_size_cells(vm_dtb, gic_offset);

	len = fdt_get_int_value(vm_dtb, gic_offset, "#redistributor-regions", &val);
	if (len < 0) {
		TRACEF("failed to find redistributor-regions\n");
		return ERR_NOT_FOUND;
	}

	cfg->redist_num = val;
	ptr = (void *)fdt_getprop(vm_dtb, gic_offset, "reg", &len);
        if (NULL == ptr) {
                TRACEF(" failed to get intc reg\n");
                return ERR_NOT_FOUND;
        }

	/* base address  */	
	cfg->d_base = fdt_next_cell(address_cell,(fdt32_t **) &ptr);
	cfg->d_len = fdt_next_cell(cell_size, (fdt32_t **)&ptr);
	cfg->r0_base = fdt_next_cell(address_cell,(fdt32_t **) &ptr);
	cfg->r0_len = fdt_next_cell(cell_size, (fdt32_t **)&ptr);
	if (cfg->redist_num == 2) {
		cfg->r1_base = fdt_next_cell(address_cell,(fdt32_t **) &ptr);
		cfg->r1_len = fdt_next_cell(cell_size, (fdt32_t **)&ptr);
	}

	return 0;
}
