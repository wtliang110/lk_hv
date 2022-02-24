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
#ifndef __HV_IRQ_H__
#define __HV_IRQ_H__
#include <hv_vm.h>
#include <hv_vcpu.h>

struct virq_info {
	struct lk_vm *vm;
	uint32_t virq;
	struct lk_vcpu *vcpu;	
};

int hv_init_virq_info(void);
enum handler_return hv_int_handler(void *arg);
int hv_map_irq_to_guest(struct lk_vm *vm, uint32_t virq, uint32_t irq);
/* how to ppi informations, about ppi assignment, 
 * please refer: A_BSA_1.0 ppi most of about timer, 
 * so it is a topic about timer virtulization  */
#endif 
