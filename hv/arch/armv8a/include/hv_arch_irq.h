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

#ifndef __HV_ARCH_IRQ__
#define __HV_ARCH_IRQ__

#include <hv_vm.h>
#include <dev/interrupt/arm_gic.h>

int hv_vm_irq_init(struct lk_vm *vm);
int hv_arch_spi_nr(void);
int hv_inject_virq(struct lk_vm *vm, struct lk_vcpu *vcpu, uint32_t virq);
#define    MIN_SPI_NO            GIC_BASE_SPI

#endif
