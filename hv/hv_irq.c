/* interrupt virtuliazation functions 
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
#include <platform/interrupts.h>
#include <hv_irq.h>
#include <stdint.h>
#include <hv_arch_irq.h>
#include <lk/trace.h>
#include <hv_irq.h>
#include <lk/err.h>
#include <string.h>
#include <hv_vgic.h>

#define LOCAL_TRACE 1

/* spi route information support max irqs, the index is irq - 32 
 * irq <----> virq <----> domain mapping  
 * those route information is only for spi */
static struct virq_info *irq2virq;

/* do we need lock ????  */
/* static spin_lock_t irq_lock = SPIN_LOCK_INITIAL_VALUE; */

/* init virq information structure  */
int hv_init_virq_info(void)
{
	int irq_nr = hv_arch_spi_nr();

	irq2virq = malloc(sizeof(struct virq_info) * irq_nr);
	if (NULL == irq2virq) {
		TRACEF("failed to allocate memory for irq3virq\n");
		return ERR_NO_MEMORY;
	}

	memset(irq2virq, 0, sizeof(struct virq_info) * irq_nr);	
	return 0;
}

/* get virq info by @irq  */
static struct virq_info *get_virq_info(uint32_t irq)
{
	if (irq < MIN_SPI_NO)
		return NULL;

	return irq2virq + (irq - MIN_SPI_NO);
}	

/* interrupts handler, 
 * will forwad interrupt to target vcpu ****/
enum handler_return hv_int_handler(void *arg)
{
	struct virq_info *virq = NULL;
	uint32_t irq = (uint32_t)arg;

	virq = get_virq_info(irq);
	if (NULL == virq) {
		TRACEF("failed to get virq information for irq %u\n", irq);
		return 0;
	}

	/* inject virq to guest */
	LTRACEF("will inject virq %u\n", virq->virq);
	hv_inject_virq(virq->vm, virq->vcpu, virq->virq);
	//gic_dump_status(irq, 0);
	return 0;
}

/* route irq to vm, the virtual interrupt is virq 
 * just make a mapping between irq<--->vm<---->virq 
 * called for map spis */
int hv_map_irq_to_guest(struct lk_vm *vm, uint32_t virq, uint32_t irq)
{
	struct virq_info *virq_map;

	virq_map = get_virq_info(irq);
	if (virq_map == NULL) {
		TRACEF("failed to get virq inforamtion for virq %u\n", irq);
		return ERR_NOT_FOUND;
	}

	virq_map->vm = vm;
	virq_map->virq = virq;

	register_int_handler(irq, hv_int_handler, (void *)INT_IRQ);
	hv_vm_init_virq(vm, virq, irq);    	
	return 0;
}

/* TODO: remove spi maping when delete a vm.
 * called during delete a vm  */
void hv_unmap_guest_irq(struct lk_vm *vm)
{
	return;
}
