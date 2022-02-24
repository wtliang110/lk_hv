/* vgic mmio operations 
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

#ifndef __VGIC_MMIO_H__
#define __VGIC_MMIO_H__

struct lk_vm;
struct lk_vcpu;
void init_gicd_reg(struct lk_vm *vm,  struct vgic_dist *dist);
int hv_dist_mmio_init(struct lk_vm *vm);
int hv_redist_mmio_init(struct lk_vcpu *vcpu);
void gic_sgi_emulation(uint64_t value);
#endif 
