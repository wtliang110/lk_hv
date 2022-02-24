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

#ifndef __HV_VGIC_H__
#define __HV_VGIC_H__

#include <lk/list.h>
#include <stdint.h>
#include <vgic_gic.h>
#include <arch/spinlock.h>
#include <sys/types.h>

enum irq_type {
	TYPE_LEVEL = 0,
	TYPE_EDGE,
};

struct lk_vcpu;
struct lk_vm;

/* arm gic virtulization based on gicv3 */
struct vgic_irq {
	struct list_node pd_node;        /* pending list which will be listed into vcpu pending list */
	struct list_node lr_node;        /* virqs list which already in lrs */
	struct lk_vcpu *vcpu;            /* the qeueu vcpu */
	uint32_t intid;                  /* guest visiable intid */
	uint32_t hwintid;                /* hw intid */
	uint8_t priority;                /* priority of this virtual irq */
	uint8_t pending:1;               /* whether pending  */
	uint8_t active:1;                /* whether active */
	uint8_t enable:1;                /* is enable of this virtual irq */
	uint8_t type:1;                  /* type of this irq: level or edge  */
	uint8_t group:1;                 /* group of this virtual irq  */
	uint8_t pdq:1;                   /* already vcpu pending list, but not in lr list */
	uint8_t migrate:1;               /* whether already migrate status */
	uint8_t lr;                      /* lr index which injected into */  
	uint64_t iroute;                 /* iroute of this virtual irq  */
	spin_lock_t irq_lock;	         /* lock protect  */
};

struct vgic_dist {
	uint32_t spi_num;                /* spi numbers  */
	struct vgic_irq *spis;           /* spi arrays */
	uint32_t gicd_ctrl;
	uint32_t gicd_typer;
	uint32_t gicd_iidr;
	uint32_t gicd_typer2;
	paddr_t  dist_base;
	uint32_t dist_len;
};

struct vgic_cfg {
	uint64_t d_base;
	uint64_t r0_base;
	uint64_t r1_base;
	uint32_t d_len;
	uint32_t r0_len;
	uint32_t r1_len;
	int redist_num;
};

struct vgic {
	struct vgic_cfg cfg;
	struct vgic_dist dist;
};

struct vgic_redist {
	struct vgic_irq irqs[32];
	struct list_node pd_head;
	struct list_node lr_head;
	spin_lock_t pd_lock;
	uint8_t used_lr;                 /* how many lr already used   */
	paddr_t rd_base;             	 /* redistributor */
	uint32_t rd_len;
	paddr_t sgi_base;
	uint64_t ich_vmcr;
	uint64_t ich_hcr;
	uint64_t lr[LR_NR];              /* list registers */
	uint64_t apr0[AP_NR];            /* each bit for a priority level, so, the max is 7bit, 32 = 4 */
	uint64_t apr1[AP_NR];            /* each bit for a priority level, so, the max is 7bit, 32 = 4 */
	uint8_t ap_nr;
	uint8_t lr_nr;                   /* lr number  */
};

void hv_vcpu_irq_save(struct lk_vcpu *vcpu);
void hv_vcpu_irq_init(struct lk_vcpu *vcpu);
void hv_vcpu_irq_restore(struct lk_vcpu *vcpu);
void hv_sync_irq_from_lrs(struct lk_vcpu *vcpu);
void hv_flush_queued_virqs(struct lk_vcpu *vcpu);
void hv_vm_init_virq(struct lk_vm *vm, uint32_t virq, uint32_t irq);
int hv_virq_enable(struct vgic_irq *virq, int enable);
struct vgic_irq *get_virq(struct lk_vm *vm, struct lk_vcpu *vcpu, uint32_t virq);
int is_virq_pending(struct vgic_irq *v_irq);
void hv_virq_cpending(struct vgic_irq *v_irq);
struct vgic_dist *hv_get_dist(struct lk_vm *vm);
void hv_virq_to_lrs(void);
void hv_virq_from_lrs(void);
void insert_into_pdlist(struct vgic_irq *virq,  struct lk_vcpu *vcpu);
void hv_vcpu_irq_cleanup(struct lk_vcpu *vcpu);
#endif
