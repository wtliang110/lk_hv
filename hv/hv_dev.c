/* device virtuliazation
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

/* device should be include following resouces:
 * 1. irq. register irq information to interrupt virtualization
 * 2. memory. map memory in stage2   
 * 3. mmio space */

#if 0
	device node including all of device
	type: pt/emu/virtio
	phandle  : phandle for physical device 
	vphandle : vphandle for virtual device, which is in vm dts
	vm@1 {
		..................................................
		/* emulation device  */
		device  {
			
			dev_num = <>;	
			intc {
				type = "emu";
				vphandle = <0x8001>;
				compatible = "arm,gic-v3";
			};

			searial {
				type = "pt";
				phandle = <0x8001>;
				compatible = "searial";
				vphandle = <0x8001>;
			};
		};
	};
#endif

/* parse deice 
 * @dev_off offset of this node 
 * @vm_dtb
 * @dev return value  */
int dev_parse_one_node(int dev_off, void *vm_dtb, struct vm_dev *dev)
{
	return 0;
}

/* parse device config  */
int hv_vm_dev_cfg_parse(struct lk_vm *vm)
{
	int val, int len;
	int child;
	void *ptr;
	int cell_size, address_cell;
	int dev_off = 0;
	struct vgic *vgic = (struct vgic *)(vm->interrupt);
	struct vgic_cfg *cfg = &(vgic->cfg);
	void *vm_dtb = vm->info->vm_cfg;

	dev_off = fdt_subnode_offset_namelen(vm_dtb, 
						vm->info->cfg_offset,
						"device", 4);
	if (dev_off < 0) {
		TRACEF("failed to find node dev %d %d\n", 
				dev_off, vm->info->cfg_offset);
		return ERR_NOT_FOUND;
	}

	len = fdt_get_int_value(vm_dtb, dev_off, "dev_num", &val);
	if ( len < 0) {
		TRACEF("failed to find dev_num\n");
		return ERR_NOT_FOUND;
	}

	vm->dev_num = val;
	len = sizeof(struct vm_dev) * val;
	vm->devs = (struct vm_dev *)malloc(len);
	if (NULL == vm->devs) {
		TRACEF("failed to alloc memory device structure\n");
		return ERR_NO_MEMORY;
	}

	memset(vm->devs, 0, len);
	len = 0;
	fdt_for_each_subnode(child, vm_dtb, dev_off)
		dev_parse_one_node(child, vm_dtb, vm->devs + len);

	return 0;
}
