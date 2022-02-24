/* gic virtulization  
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
#include <kernel/vm.h>
#include <sys/types.h>
#include <arch/arm64.h>
#include <string.h>
#include <hv_arch_vcpu.h>
#include <hv_vcpu.h>
#include <hv_vm.h>
#include <dev/interrupt/arm_gic.h>
#include <hv_vgic.h>
#include <vgic_gic.h>
#include <kernel/spinlock.h>
#include <lk/list.h>
#include <lk/trace.h>
#include <hv_arch_irq.h>
#include <hv_arch_cfg.h>
#include <vgic_mmio.h>
#include <hv_event.h>
#define LOCAL_TRACE 1

#define lock_redist(redist)    spin_lock_irqsave(&((redist)->pd_lock), statep)
#define unlock_redist(redist)  spin_unlock_irqrestore(&((redist)->pd_lock), statep)

/* some utiles  */
int hv_arch_spi_nr(void)
{
	return get_max_spi();
}

struct vgic_dist *hv_get_dist(struct lk_vm *vm)
{
	struct vgic *vgic = NULL;
	struct vgic_dist *dist = NULL;

	vgic = (struct vgic *)vm->interrupt;
	dist = (struct vgic_dist *)(&(vgic->dist));
	return dist;
}

#define  VIRQ_DEFAULT_PRI   100
static void init_virq(struct vgic_irq *p_virq, uint32_t virq, uint32_t irq)
{
	memset(p_virq, 0, sizeof(*p_virq));
	p_virq->hwintid = irq;
	p_virq->intid = virq;
	list_initialize(&(p_virq->pd_node));
	list_initialize(&(p_virq->lr_node));
	spin_lock_init(&(p_virq->irq_lock));
	p_virq->lr = LR_INDEX_INVALID;
	p_virq->priority = VIRQ_DEFAULT_PRI;
	p_virq->group = 1;
}

/* init irq of @vm */
int hv_vm_irq_init(struct lk_vm *vm)
{
	int spi_nr = 0;
	int len = 0, ret = 0;
	struct vgic *vgic = NULL;
	struct vgic_dist *dist = NULL;
	struct vgic_cfg *cfg = NULL;

	spi_nr = get_max_spi();
	LTRACEF("init vm interrupt structure spi nr is %d\n", spi_nr);
	len = sizeof(*vgic) + sizeof(struct vgic_irq) * spi_nr;
	vgic = malloc(len);
	if (NULL == vgic) {
		TRACEF("failed to allocate vgic dist for vm\n");
		return ERR_NO_MEMORY;
	}

	memset(vgic, 0, len);
	dist = &(vgic->dist);
	dist->spis = (struct vgic_irq *)(vgic + 1);
	dist->spi_num = spi_nr;
	vm->interrupt = vgic;

	/* int gic cfg  */
	ret = hv_vm_irq_cfg_get(vm);
	cfg = &(vgic->cfg);
	dist->dist_base = cfg->d_base;
	dist->dist_len = cfg->d_len;
	LTRACEF("dist base is %lx len %u\n", dist->dist_base, dist->dist_len);

	/* init registers */
	init_gicd_reg(vm, dist);			
	ret = hv_dist_mmio_init(vm);
	for (len = 0; len < (int)dist->spi_num; len++) {
		struct vgic_irq *v_irq = dist->spis + len;
		init_virq(v_irq, 0, 0);
	}

	return ret;
}

void hv_vm_init_virq(struct lk_vm *vm, uint32_t virq, uint32_t irq)
{
	uint32_t irq_index = 0;
	struct vgic_irq *v_irq = NULL;
	struct vgic_dist *dist = NULL;

	if (virq < MIN_SPI_NO) {
		TRACEF("virq %u is not share irq\n", virq);
		return;
	}

	dist = hv_get_dist(vm);
	irq_index = virq - MIN_SPI_NO;
	LTRACEF("dist is %p index %u vm is %p\n", dist, irq_index, vm);
	if (irq_index > dist->spi_num) {
		TRACEF("virq is wrong %u\n", virq);
		return;	
	}
		
	v_irq = dist->spis + irq_index;
	init_virq(v_irq, virq, irq);

	/* default dest vcpu 0  */
	v_irq->vcpu = vm->vcpu[0];
	LTRACEF("allocate vgic_irq for virq %u, irq %u\n", virq, irq);
}	

/* free the resource  */
void hv_vm_irq_deinit(struct lk_vm *vm)
{
	free(vm->interrupt);
	vm->interrupt = NULL;
	return;
}

/* init vgic_redist and other structures  */
void hv_vcpu_irq_init(struct lk_vcpu *vcpu)
{
	int i = 0, pribits = 0;
	struct vgic_cfg *cfg = NULL;
	struct lk_vm *vm = NULL;
	struct vgic *vgic = NULL;
	struct vgic_redist *vredist = &(vcpu->arch_vcpu.vgic);

	memset(vredist, 0, sizeof(*vredist));
	list_initialize(&(vredist->pd_head));
	list_initialize(&(vredist->lr_head));
	vredist->ich_hcr = ICH_HCR_EN;
	vredist->ich_vmcr = ICH_VMCR_VENG1; 
	spin_lock_init(&(vredist->pd_lock));

	/* vredist start ipa */
	vm = vcpu->vm;
	vgic = (struct vgic *)(vm->interrupt);
	cfg = &(vgic->cfg);
	vredist->rd_base = cfg->r0_base +  vcpu->id * 2 * GICR_FRAME; 
	vredist->rd_len = GICR_FRAME * 2;
	LTRACEF("redist base is %lx len %u\n", vredist->rd_base, vredist->rd_len);
	vredist->sgi_base = cfg->r0_base + (vcpu->id * 2 + 1) * GICR_FRAME;
	LTRACEF("redist sgi_base is %lx\n", vredist->sgi_base);

	pribits = vgic_get_pribits();
	if (pribits < 5)
		vredist->ap_nr = 1;
	else
		vredist->ap_nr = (1 << (pribits - 5));
	
	vredist->lr_nr = vgic_get_lr_nr();
	LTRACEF("there are ap is %d %d\n", vredist->ap_nr, vgic_get_pribits());
       	
	hv_redist_mmio_init(vcpu);
	
	for (i = 0; i < 32; i++) {
		struct vgic_irq *virq = vredist->irqs + i;
		init_virq(virq, i, i);
	}

	return;
}

/* irq cleanup during vcpu poweroff  */
void hv_vcpu_irq_cleanup(struct lk_vcpu *vcpu)
{
	int i = 0;
	struct vgic_irq *virq = NULL, *virq_tmp = NULL;
	struct vgic_redist *vredist = &(vcpu->arch_vcpu.vgic);
	struct gic_lr lr_val;
	spin_lock_saved_state_t statep;

	LTRACEF("cleanup lr_head\n");
	lock_redist(vredist);
	list_for_every_entry_safe(&(vredist->lr_head), virq, virq_tmp, struct vgic_irq, lr_node) {
		list_delete(&(virq->lr_node));
		list_initialize(&(virq->lr_node));		
	}
	list_initialize(&(vredist->lr_head));		
	unlock_redist(vredist);
	
	/* lr cleenup */
	for (i = 0; i < vredist->lr_nr; i++) {
		vgic_read_lr(i, &lr_val);
		if (lr_val.pirq) {
			gic_set_icactiver(lr_val.pirq);
			gic_clear_pending(lr_val.pirq);
		}
		vgic_write_lr_reg(i, 0);
	}

	/* pending list cleanup  */
	LTRACEF("cleanup pd_head\n");
	lock_redist(vredist);
	list_for_every_entry_safe(&(vredist->pd_head), virq, virq_tmp, struct vgic_irq, pd_node) {
		LTRACEF("cleanup pirq %p\n", virq);
		list_delete(&(virq->pd_node));
		list_initialize(&(virq->pd_node));
		if (virq->hwintid)
			gic_set_icactiver(virq->hwintid);
			
	}
	list_initialize(&(vredist->pd_head));		
	unlock_redist(vredist);
}

/* virtual irq save during vcpu switch out  
 * save : lrs/hcr/vmcr */
void hv_vcpu_irq_save(struct lk_vcpu *vcpu)
{
	int i = 0;
	struct vgic_redist *vredist = &(vcpu->arch_vcpu.vgic);

	/* apNrN, do we need to save this  */
	for (i = 0; i < vredist->ap_nr; i++) {
		vredist->apr0[i] = vgic_read_aprn_reg(i, 0);
		vredist->apr1[i] = vgic_read_aprn_reg(i, 1);
	}

	vredist->ich_vmcr = ARM64_READ_SYSREG(ICH_VMCR_EL2);
	vredist->ich_hcr = ARM64_READ_SYSREG(ICH_HCR_EL2);

	/* sync it from lrs first need it ??????? */
	/* hv_sync_irq_from_lrs(vcpu); */
	for (i = 0; i < vredist->lr_nr; i++) {
		vredist->lr[i] = vgic_read_lr_reg(i);  
	}	
	return;
}

void hv_vcpu_irq_test(void)
{
	/* enable vcpu interface */
	ARM64_WRITE_SYSREG(ICH_VMCR_EL2, ICH_VMCR_VENG1);
	ARM64_WRITE_SYSREG(ICH_HCR_EL2, ICH_HCR_EN);	
}

/* processor of vcpu is changed, so need to change 
 * the virq affinity to this vcpu's affinity  */
static void vcpu_move_irqs(struct lk_vcpu *vcpu)
{
	int i = 0;
	struct vgic_irq *virq = NULL;
	struct vgic_dist *dist = NULL;

	dist = hv_get_dist(vcpu->vm);
	for (i = 0; i < (int)dist->spi_num; i++) {
		virq = dist->spis + i;
		if (virq->vcpu != vcpu ||
		    0 == virq->hwintid)
			continue;
	
		/* migrating, wait for EOI  */	
		if (virq->migrate)
			continue;

		gic_set_affinity(virq->hwintid, vcpu->processor, 0);
	}
	return;
}

/* virtual irq restore during vcpu switch out  */
void hv_vcpu_irq_restore(struct lk_vcpu *vcpu)
{
	int32_t cpu_id = -1;
	int i = 0;
	struct vgic_redist *vredist = &(vcpu->arch_vcpu.vgic);

	ARM64_WRITE_SYSREG(ICH_VMCR_EL2, vredist->ich_vmcr);
	ARM64_WRITE_SYSREG(ICH_HCR_EL2, vredist->ich_hcr);
		
	for (i = 0; i < vredist->ap_nr; i++) {
		vgic_write_aprn_reg(i, 0, vredist->apr0[i]);
		vgic_write_aprn_reg(i, 1, vredist->apr1[i]);
	}
	
	for (i = 0; i < vredist->lr_nr; i++) {
		vgic_write_lr_reg(i, vredist->lr[i]);  
	}

	/* check whether pcpu which vcpu is running on changed, 
	 * if changed, do virq moves  */
	cpu_id = hv_vcpu_cpu_id(vcpu);
	if (0 > cpu_id)
		return;

	if (cpu_id != (int)vcpu->processor) {
		vcpu->processor = cpu_id;
		vcpu_move_irqs(vcpu);
	}

	return;
}


/* sync status from lrs to pending irqs 
 * will be called before enter hypervisor from guest */
/* vcpu is current vcpu, just syn virq from sw  
 * call this functions :
 * 1. before enter into hypvisor 
 *    ---before irq handler 
 *    ---before excep handler */
void hv_sync_irq_from_lrs(struct lk_vcpu *vcpu)
{
	int free = 0;
	struct gic_lr lr_val;
	struct vgic_irq *virq, *virq_tmp;
	struct vgic_redist *vredist = &(vcpu->arch_vcpu.vgic);
	spin_lock_saved_state_t statep;

	/* disable current interrupt  */
	lock_redist(vredist);	
	list_for_every_entry_safe(&(vredist->lr_head), virq, virq_tmp, struct vgic_irq, lr_node) {

		/* virq is not invalid or it's a  hw interrupt 
		 * LR state for virtual interrupt from hw is maintained by distributor
		 * but not virtual interface, so sw can not change its state */
		if (virq->lr == LR_INDEX_INVALID)
			continue;

		vgic_read_lr(virq->lr, &lr_val);

		/* check whether it is already used by others  */
		if (virq->intid != lr_val.virq) {
			virq->pending = 0;
			virq->active = 0;
			virq->lr = 0;
			list_delete(&(virq->lr_node));
			list_initialize(&(virq->lr_node));
			if (virq->migrate) {
				int cpu_id = hv_vcpu_cpu_id(virq->vcpu);
				gic_set_affinity(virq->hwintid, cpu_id, 0);
				virq->migrate = 0;
			}
			LTRACEF("free virq %d\n", virq->intid);
			free = 1;
			continue;
		}

		switch (lr_val.status) {
			case LR_ST_INVALID:
				virq->pending = 0;
				virq->active = 0;
				virq->lr = 0;
				list_delete(&(virq->lr_node));
				list_initialize(&(virq->lr_node));
				if (virq->migrate) {
					int cpu_id = hv_vcpu_cpu_id(virq->vcpu);
					gic_set_affinity(virq->hwintid, cpu_id, 0);
					virq->migrate = 0;
				}
				LTRACEF("free virq %d\n", virq->intid);
				free = 1;
				break;

			case LR_ST_PENDING:
				virq->pending = 1;
				virq->active = 0;
				break;

			case LR_ST_ACTIVE:
				virq->pending = 0;
				virq->active = 1;
				free = 1;
				break;

			case LR_ST_ACT_PEND:
				virq->pending = 1;
				virq->active = 1;
				break;

			default:               /* status is wrong */
				continue;
		}
	}
	unlock_redist(vredist);

	if (free)
		vgic_maintenace_off(MAINT_UIE);

	return;
}

/* move from pending to lr list  */
static void move_to_lr_lst(struct vgic_redist *dist, struct vgic_irq *virq)
{
	list_delete(&(virq->pd_node));
	list_initialize(&(virq->pd_node));
	virq->pdq = 0;
	smp_wmb();
	if (list_is_empty(&(virq->lr_node)))
		list_add_tail(&(dist->lr_head), &(virq->lr_node));

	return;
}	

/* flush virqs in pending list to lrs 
 * call this function :
 * 1. before enter into guest
 *    ----after finish irq handler
 *    ----after finish excep handler
 * 2. how this happend after shedule  */
void hv_flush_queued_virqs(struct lk_vcpu *vcpu)
{
	struct vgic_irq *virq;
	struct vgic_irq *tmp_virq;
	struct vgic_redist *vredist = &(vcpu->arch_vcpu.vgic);
	spin_lock_saved_state_t statep;

	/* no queued virq, return */
	if (list_is_empty(&(vredist->pd_head))) {
		//LTRACEF("flush irq into lrs %p, no pending list\n", vcpu);
		return;
	}

	lock_redist(vredist);
	list_for_every_entry_safe(&(vredist->pd_head), virq, tmp_virq, struct vgic_irq, pd_node) {
		LTRACEF("virq is %p\n", virq);
			
		/* interrupt from hw */
		if (virq->hwintid) {
			virq->lr = vgic_get_lr();
			LTRACEF("get lr %d for virq %u\n", virq->lr, virq->intid);

			if (LR_INDEX_INVALID == virq->lr) {
				LTRACEF("failed to get lr for virq %u\n", virq->intid);
				LTRACEF("virq %u state is QUEUED & no pending\n", virq->intid);						
				/*  there is no free lrs, turn on mit, then return  */
				vgic_maintenace_on(MAINT_UIE);
				goto out;
			}

			LTRACEF("write virq to lr, lr %d for virq %u\n", virq->lr, virq->intid);
			vgic_write_lr(virq);
			move_to_lr_lst(vredist, virq);
			continue;
		} else {
			/* interrupt from sw  */
			if (virq->lr == LR_INDEX_INVALID) {
				/* =======>>>>>>>pending  */
				virq->lr = vgic_get_lr();
				if (LR_INDEX_INVALID == virq->lr) {
					LTRACEF("failed to get lr for virq %u\n", virq->intid);
					LTRACEF("virq %u state is QUEUED & no pending\n", virq->intid);
					/*  there is no free lrs, turn on mit, then return  */
					vgic_maintenace_on(MAINT_UIE);
					goto out;
				}
				
				LTRACEF("write virq %d to lr %d\n", virq->intid, virq->lr);
				vgic_write_lr(virq);
				virq->pending = 1;
				move_to_lr_lst(vredist, virq);
			} else  {
				/* =======>>>>>>>active & pending  */
				struct gic_lr v_lr;

				vgic_read_lr(virq->lr, &v_lr);
				if (v_lr.status == LR_ST_ACTIVE) {
					/* =======>>>>>>>active & pending  */
					LTRACEF("virq %d to is active\n", virq->intid);
					vgic_update_lr(virq);
					if (!virq->active)
						virq->active = 1;
					virq->pending = 1;
					move_to_lr_lst(vredist, virq);
				} else if (v_lr.status == LR_ST_INVALID) {
					LTRACEF("virq %d to is deactive\n", virq->intid);
					/* maybe already EOIed by guest */
					vgic_write_lr(virq);
					virq->pending = 1;
					move_to_lr_lst(vredist, virq);
				} else {
					/* can not update lr, do nothing  */
				}
			}
		}
	}

out:
	unlock_redist(vredist);	
	return;
}

void insert_into_pdlist(struct vgic_irq *virq,  struct lk_vcpu *vcpu)
{
	struct vgic_redist *vredist;
	spin_lock_saved_state_t statep;

	
	/* list is not in pending list insert it into pending list by order */
	LTRACEF("queue irq %u to  vcpu %p  pending list\n", virq->intid, vcpu);
	vredist = &(vcpu->arch_vcpu.vgic);
	
	lock_redist(vredist);
	if (!list_is_empty(&(virq->pd_node))) { /* already in the pd list, return */
		unlock_redist(vredist);
		LTRACEF("irq is already in pending list\n");
		/* TODO check again, maybe   */
		return;
	}
	
	virq->pdq = 1;
	smp_wmb();

	if (list_is_empty(&(vredist->pd_head))) {
		LTRACEF("list add tail %p\n", virq);
		list_add_tail(&(vredist->pd_head), &(virq->pd_node));
	} else {
		struct vgic_irq *tmp;
		LTRACEF("list is not empty\n");
		list_for_every_entry(&(vredist->pd_head), tmp, struct vgic_irq, pd_node) {
			if (tmp->priority < virq->priority) {
				list_add_before(&(tmp->pd_node), &(virq->pd_node));
				break;
			}
		}	
	}
	unlock_redist(vredist);
}

/* inject virq to  @vcpu :
 * cases :
 * 1. vcpu is currently running vcpu
 * 2. vpcu is not currently running vcpu
 * 3. virq status change : 
 * solutions:
 * 1. IRQ_QUEUE: in vcpu pending list 
 * 2. IRQ_PENDING : put into lrs, guest not ack it 
 * 3. IRQ_ACTIVE:  already in lrs, guest already ack it 
 * 4. how to know pending--->active from other cpu, does not care it first
 *
 *  (init)
 *    |
 *    |
 *    |
 *(insert into vcpu pending list)
 *    |
 *    |
 *    V
 * IRQ_QUEUE---(insert into lrs : lr list)---------->IRQ_PENDING
 *                                                    |
 *                                                    |
 *                                               (acked by guest)
 *                                                    |
 *                                                    |
 *                                                    V
 *                                              IRQ_ACTIVE */
static int hv_vcpu_inject_virq(struct lk_vm *vm, struct lk_vcpu *vcpu, struct vgic_irq *virq)
{
	/* currently is disabled  */
	if (!virq->enable) {
		LTRACEF("irq %u is not enalbed %p\n", virq->intid, virq);
		return -1;
	}

	/* check vcpu status : if vcpu is down, return */
	if (is_vcpu_poweroff(vcpu))
		return -1;

	/* already in  */
	smp_rmb();
	if (virq->pdq) {
		LTRACEF("irq %u is already in vcpu %p  pending list\n", 
				virq->intid, vcpu);
		return 0;
	}
	
	/* list is not in pending list insert it into pending list by order */
	LTRACEF("queue irq %u to  vcpu %p  pending list\n", virq->intid, vcpu);
	insert_into_pdlist(virq, vcpu);

	/* if vcpu is running on other core, send event virq to that core */
	/* if vcpu is running on the current core, will inject when back to guest mode */
	if (is_vcpu_running(vcpu) && 
		vcpu != current_vcpu()) {
		int cpu_id = hv_vcpu_cpu_id(vcpu); 
		if (cpu_id < 0) {
			TRACEF("failed to get cpu id for vcpu %p\n", vcpu);
			return -1;
		}

		hv_empty_event(cpu_id);
		return 0;
	}
	
	/* if vcpu is waiting for event, such as into a lower power state.
	 * kick vcpu to receive interrupts */
	if (is_vcpu_lowpower(vcpu)) {
		LTRACEF("vcpu is in low power state, resume it\n");
		hv_vcpu_resume(vcpu);
	}

	return 0;
}

struct vgic_irq *get_virq(struct lk_vm *vm, struct lk_vcpu *vcpu, uint32_t virq)
{
	struct vgic_irq *v_irq = NULL;

	if (virq < GIC_BASE_SPI) {
		struct vgic_redist *vredist = NULL;
		
		if (NULL == vcpu) {
			TRACEF("target vcpu is NULL for irq %u\n", virq);
			return NULL;
		}
		vredist = &(vcpu->arch_vcpu.vgic);
		v_irq = vredist->irqs + virq;
	} else {
		uint32_t index = 0; 
		struct vgic_dist *vdist;

		vdist = hv_get_dist(vm);
		index = virq - GIC_BASE_SPI;
		if (index >= vdist->spi_num)
			return NULL;

		v_irq = vdist->spis + index;
	//	LTRACEF("virq %u is spi\n", virq);
	}

	return v_irq;
}

/* inject @virq to @vm or @vcpu */
/* vgic_irq->vcpu set by iroute, default one is first cpu */
int hv_inject_virq(struct lk_vm *vm, struct lk_vcpu *vcpu, uint32_t virq)
{
	int ret = 0;
	struct vgic_irq *v_irq;

	LTRACEF("inject virq %u to  vcpu %p \n", virq, vcpu);
	v_irq = get_virq(vm, vcpu, virq);
	if (NULL == v_irq) {
		TRACEF("failed to get vgic_irq for virq %u\n", virq);
		return ERR_INVALID_ARGS;
	}

	/* for spi, vcpu may be NULL, get it from virq structure  */
	if (vcpu == NULL)
		vcpu = v_irq->vcpu;

	/* failed to inject virq, need to deactive the pirq  */
	ret = hv_vcpu_inject_virq(vm, vcpu, v_irq);
	if (ret < 0 && v_irq->hwintid) {
		  gic_set_icactiver(v_irq->hwintid);
	}

	return ret;
}

/* clearing pending irq  */
void hv_virq_cpending(struct vgic_irq *v_irq)
{
	v_irq->pdq = 0;
	if (!list_is_empty(&(v_irq->pd_node))) {
		list_delete(&(v_irq->pd_node));
		list_initialize(&(v_irq->pd_node));
		v_irq->pending = 0;
		v_irq->active = 0;

		/* TODO: any others : lrs */
	}
}

/* 1. does not support a virq to be handled by 2 vcpus
 * 2. if the virq is in lr, wait for EOI at then migrate at that time
 * 3. if the virq is in pd list , but not in lr, migrate it directly 
 * 4. if the virq is not in pd list, do nothing
 * 5. if the virq is not hw related, do nothing */
int hv_migrate_virq(struct lk_vcpu *o_vcpu, struct lk_vcpu *n_vcpu, struct vgic_irq *v_irq)
{
	int cpu_id = hv_vcpu_cpu_id(n_vcpu);
	struct vgic_redist *vredist = &(o_vcpu->arch_vcpu.vgic);
	spin_lock_saved_state_t statep;

	/* this is a sw irq, do nothing  */
	if (v_irq->hwintid == 0) { 
		v_irq->vcpu = n_vcpu;
		return 0;
	}

	/* already in migrating  */
	if (v_irq->migrate)
		return -1;

	lock_redist(vredist);
	
	/* does not in any queue  */
	if (list_is_empty(&v_irq->pd_node)) {
		gic_set_affinity(v_irq->hwintid, cpu_id, 0);
		v_irq->vcpu = n_vcpu;
		goto out;	
	} 

	/* already in lr, wait for EOI  */
	if (!list_is_empty(&v_irq->lr_node)) {
		v_irq->vcpu = n_vcpu;
		v_irq->migrate = 1;
		goto out;
	}

	/* in pdq, but not in lr  */
	if (v_irq->pdq) {
		v_irq->vcpu = n_vcpu;
		list_delete(&(v_irq->pd_node));
		list_initialize(&(v_irq->pd_node));
		v_irq->pdq = 0;
		gic_set_affinity(v_irq->hwintid, cpu_id, 0);
		unlock_redist(vredist);
		hv_vcpu_inject_virq(n_vcpu->vm, n_vcpu, v_irq);
		return 0;	
	}

out:
	unlock_redist(vredist);
	return 0;
}

/* set irq enable */
int hv_virq_enable(struct vgic_irq *v_irq, int enable)
{
	if (v_irq == NULL) {
		TRACEF("failed to get vgic_irq for virq %u\n", v_irq->hwintid);
		return ERR_INVALID_ARGS;
	}
	
	v_irq->enable = !!enable;

	/* if this is a hw irq, enable hw irq */
	if (v_irq->hwintid && enable) {
		//printf("enable %d type is %d\n", v_irq->hwintid, v_irq->type);
		//gic_set_cfg(v_irq->hwintid, v_irq->type == TYPE_EDGE, arch_curr_cpu_num());
		gic_set_enable(v_irq->hwintid, v_irq->enable);
	}

	if (!enable)
		hv_virq_cpending(v_irq);
		
	return 0;
}

int is_virq_pending(struct vgic_irq *v_irq)
{
	return v_irq->pdq || v_irq->pending;
}

/* inject virq of current vcpu into lrs */
void hv_virq_to_lrs(void)
{
	struct lk_vcpu *vcpu = current_vcpu();

	/* no vcpu or vcpu is not running, return */
	if (NULL == vcpu ||
		vcpu->is_inited == 0)
		return;

	/* disable interrupt  */
	hv_flush_queued_virqs(vcpu);
}

/* sync virq of current vcpu from lrs  */
void hv_virq_from_lrs(void)
{
	struct lk_vcpu *vcpu = current_vcpu();

	/* no vcpu or vcpu is not running, return */
	if (NULL == vcpu ||
		vcpu->is_inited == 0)
		return;

	/* disable interrupt  */
	hv_sync_irq_from_lrs(vcpu);
	
}

/* TODO lpi */
