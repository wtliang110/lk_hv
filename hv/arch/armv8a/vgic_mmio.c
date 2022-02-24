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

/* based on gic_v3   */
#include <lk/err.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <hv_arch_vcpu.h>
#include <hv_arch_irq.h>
#include <hv_vcpu.h>
#include <hv_vm.h>
#include <dev/interrupt/arm_gic.h>
#include <hv_vgic.h>
#include <vgic_gic.h>
#include <lk/trace.h>
#include <vgic_mmio.h>
#define LOCAL_TRACE 0

#define GICD_IGROUP_BASE 0x80
#define GICD_ISENAB_BASE 0x100
#define GICD_ICENAB_BASE 0x180
#define GICD_ISPEND_BASE 0x200
#define GICD_ICPEND_BASE 0x280
#define GICD_ISACTIVER_BASE 0x300
#define GICD_ICACTIVER_BASE 0x380
#define GICD_IPPRI_BASE     0x400
#define GICD_ICFGR_BASE     0x0c00
#define GICD_IROUTER_BASE   0x6100

/* gicd map */
#define GICD_CTLR         0x0
#define GICD_TYPER        0x4
#define GICD_IIDR         0x8
#define GICD_TYPER2       0xc
#define GICD_STATUSR      0x10
#define GICD_SETSPI_NSR   0x40
#define GICD_CLRSPI_NSR   0x48
#define GICD_SETSPI_SR    0x50
#define GICD_CLRSPI_SR    0x58
#define GICD_IGROUPR      0x80 ... 0x00FC
#define GICD_ISENABLER    0x0100 ... 0x017c
#define GICD_ICENABLER    0x0180 ... 0x01fc
#define GICD_ISPENDR      0x0200 ... 0x027c
#define GICD_ICPENDR      0x0280 ... 0x02fc
#define GICD_ISACTIVER    0x0300 ... 0x037c
#define GICD_ICACTIVER    0x0380 ... 0x03fc
#define GICD_IPRIORITYR   0x0400 ... 0x07f8
#define GICD_ITARGETSR    0x0800 ... 0x0bf8
#define GICD_ICFGR        0x0c00 ... 0x0cfc
#define GICD_IGRPMODR     0x0D00 ... 0x0D7C
#define GICD_NSACR        0x0e00 ... 0x0efc
#define GICD_SGIR         0x0f00
#define GICD_CPENDSGIR    0x0F10 ... 0x0F1C
#define GICD_SPENDSGIR    0x0F20 ... 0x0F2C
#define GICD_IGROUPR_E    0x1000 ... 0x107C
#define GICD_ISENABLER_E  0x1200 ... 0x127C
#define GICD_ICENABLER_E  0x1400 ... 0x147C
#define GICD_ISPENDR_E    0x1600 ... 0x167C
#define GICD_ICPENDR_E    0x1800 ... 0x187C
#define GICD_ISACTIVER_E  0x1A00 ... 0x1A7C
#define GICD_ICACTIVER_E  0x1C00 ... 0x1C7C
#define GICD_IPRIORITYR_E 0x2000 ... 0x23FC
#define GICD_ICFGR_E      0x3000 ... 0x30FC
#define GICD_IGRPMODR_E   0x3400 ... 0x347C
#define GICD_NSACR_E      0x3600 ... 0x367C
#define GICD_IROUTER      0x6100 ... 0x7FD8
#define GICD_IROUTER_E    0x8000 ... 0x9FFC

/* RD base  */
#define GICR_CTLR         0x0       /* rw */
#define GICR_IIDR         0x4       /* ro  */
#define GICR_TYPER        0x8       /* ro */
#define GICR_STATUSR      0x10      /* rw */
#define GICR_WAKER        0x14      /* rw */
#define GICR_MPAMIDR      0x18      /* ro */
#define GICR_PARTIDR      0x1c      /* rw  */
#define GICR_SETLPIR      0x40      /* wo  */
#define GICR_CLRLPIR      0x48      /* wo */
#define GICR_PROPBASER    0x70      /* rw */
#define GICR_PENDBASER    0x78      /* rw */
#define GICR_INVLPIR      0xa0      /* wo */
#define GICR_INVALLR      0xb0      /* wo  */
#define GICR_SYNCR        0xc0      /* ro */

/* currently does not supprt lpi */

/* VLPI_base ignore which do not support in v3 */

/* SGI base */
#define GICR_IGROUPR0     0x80 
#define GICR_IGROUPR_E    0x0084 ... 0x0088
#define GICR_ISENABLER0   0x0100
#define GICR_ISENABLER_E  0x0104 ... 0x0108
#define GICR_ICENABLER_E  0x0184 ... 0x0188
#define GICR_ICENABLER0   0x0180
#define GICR_ISPENDR0     0x0200
#define GICR_ISPENDR_E    0x0204 ... 0x0208
#define GICR_ICPENDR0     0x0280
#define GICR_ICPENDR_E    0x0284 ... 0x0288
#define GICR_ISACTIVER0   0x0300
#define GICR_ISACTIVER_E  0x0304 ... 0x0308
#define GICR_ICACTIVER0   0x0380
#define GICR_ICACTIVER_E  0x0384 ... 0x0388
#define GICR_IPRIORITYR   0x0400 ... 0x041C
#define GICR_IPRIORITYR_E 0x0420 ... 0x045C
#define GICR_ICFGR0       0x0C00
#define GICR_ICFGR1       0x0C04
#define GICR_ICFGR_E      0x0C08 ... 0x0C14
#define GICR_IGRPMODR0    0x0D00
#define GICR_IGRPMODR_E   0x0D04 ... 0x0D08
#define GICR_NSACR        0x0e00

void init_gicd_reg(struct lk_vm *vm,  struct vgic_dist *dist)
{
	/* currently does not support lpi, no extend irq  */
	/* rss == 0, only support 16 of aff0 */
	dist->gicd_typer = (dist->spi_num + 1) >> 5 |
				(vm->info->vcpu_num - 1);
	dist->gicd_typer2 = 0;
	dist->gicd_iidr = read_gicd_iidr();

	return;
}

/*
 *1. disable interrupt/clear pending interrrupt/clear active interrupt 
 *   if there are active/pending virtual interrupt in LR, how to disabled id/and clearn it.????????
 *
 * */


/* 
 * @off is the offset to dist/redist base 
 * @base the base of the register set range 
 * @bits is how many irq a uint32_t can express  */
static int get_irq_no(uint32_t off, uint32_t base, int bits)
{
	return (off - base) << (bits - 2);
}

static uint32_t igroupr_read(struct lk_vcpu *vcpu, uint32_t index) 
{
	int irq = 0;
	int i = 0;
	uint32_t value = 0;
	struct vgic_irq *virq;
	
	for (i = 0; i < 32; i++) {
		irq = index + i;
		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq != NULL) 
			value = value | (virq->group << i);
	}

	LTRACEF("igroutr read inedx %u value %x\n", index, value);
	return value;
}

static void igroupr_write(struct lk_vcpu *vcpu, int index, uint32_t value) 
{
	int irq = 0;
	int i = 0;
	struct vgic_irq *virq;

	LTRACEF("igroutr write inedx %u value %x\n", index, value);
	for (i = 0; i < 32; i++) {
		irq = index + i;
		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq != NULL) {
			uint8_t group =  ((value >> i) & 1);
			if (virq->group != group) {
				virq->group = group;
#if 0	
				/* currently we do not this , because all of interrupt is ng1 */
				if (virq->hwintid) { 
				/* set group of hw irq  */
					
				}
#endif 
			}
		}	
	}
}

static uint32_t isenabler_read(struct lk_vcpu *vcpu, int index) 
{
	int irq = 0;
	int i = 0;
	uint32_t value = 0;
	struct vgic_irq *virq;

	for (i = 0; i < 32; i++) {
		irq = index + i;
		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq != NULL) 
			value = value | (virq->enable << i);
	}

	LTRACEF("isenabler read inedx %u value %x\n", index, value);
	return value;
}

static void isenabler_write(struct lk_vcpu *vcpu, int index, uint32_t value) 
{
	int irq = 0;
	int i = 0;
	struct vgic_irq *virq;

	LTRACEF("isenabler write inedx %u value %x\n", index, value);
	for (i = 0; i < 32; i++) {
		irq = index + i;
		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq != NULL) {
			uint8_t enable =  ((value >> i) & 1);

			/* ignore 0 */
			if (!enable)
				continue;

			if (virq->enable == 0) {
				virq->enable = 1;
				hv_virq_enable(virq, 1);
			}
		}	
	}
}

static uint32_t icenabler_read(struct lk_vcpu *vcpu, int index) 
{
	int irq = 0;
	int i = 0;
	uint32_t value = 0;
	struct vgic_irq *virq;

	for (i = 0; i < 32; i++) {
		irq = index + i;
		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq != NULL) 
			value = value | (!(virq->enable) << i);
	}
	LTRACEF("icenabler read inedx %u value %x\n", index, value);

	return value;
}

static void icenabler_write(struct lk_vcpu *vcpu, int index, uint32_t value) 
{
	int irq = 0;
	int i = 0;
	struct vgic_irq *virq;

	LTRACEF("icenabler write inedx %u value %x vcpu %p \n", index, value, vcpu);
	for (i = 0; i < 32; i++) {
		irq = index + i;
		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq != NULL) {
			uint8_t disable =  ((value >> i) & 1);

			/* ignore 0 */
			if (!disable)
				continue;

			if (virq->enable) {
				virq->enable = 0;
				hv_virq_enable(virq, 0);	
			}
		}	
	}
}

static void  ispendr_write(struct lk_vcpu *vcpu, int index, uint32_t value)
{
	int irq = 0;
	int i = 0;
	struct vgic_irq *virq;

	LTRACEF("ispendr read inedx %u value %x\n", index, value);
	for (i = 0; i < 32; i++) {
		irq = index + i;
		if (!(value & (1 << i)) ||
			irq < GIC_BASE_PPI)
			continue;

		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq == NULL || 
			!virq->enable ||              /*  disabled */
			is_virq_pending(virq))
			continue;

		if (virq->hwintid)
			gic_trigger_interrupt(virq->hwintid);
		else
			hv_inject_virq(vcpu->vm, vcpu, irq);
	}
	
	return;
}

/* reading pending irq  */
static uint32_t ispendr_read(struct lk_vcpu *vcpu, int index)
{
	int irq = 0;
	int i = 0;
	uint32_t mask = 0;
	struct vgic_irq *virq;

	for (i = 0; i < 32; i++) {
		irq = index + i;

		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq == NULL)            /*  disabled */
			continue;

		if (virq->hwintid) {
			struct gic_lr l_val;

			/* 1. if virq is injected into lr, check lr status
			 * 2. check already in pending list 
			 * 3. others check ispendr registers  */
			if (virq->lr != LR_INDEX_INVALID &&
				virq->intid == l_val.virq) {
			       	vgic_read_lr(virq->lr, &l_val);
				if (l_val.status & LR_ST_PENDING)
					mask = mask | 1 << i;
			} else if (virq->pdq ||
				   gic_read_ispendr(virq->hwintid))
					mask = mask | 1 << i;

		} else if (is_virq_pending(virq))
			mask = mask | 1 << i;
	}
	
	LTRACEF("ispendr read inedx %u value %x\n", index, mask);
	return mask;
}

static void  icpendr_write(struct lk_vcpu *vcpu, int index, uint32_t value)
{
	int irq = 0;
	int i = 0;
	struct vgic_irq *virq;

	LTRACEF("ispendr write inedx %u value %x\n", index, value);
	for (i = 0; i < 32; i++) {
		irq = index + i;
		if (!(value & (1 << i)) ||
			irq < GIC_BASE_PPI)
			continue;
		
		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq == NULL || 
			!virq->enable)            /*  disabled */
			continue;
		
		/*  clear pendig  */
		hv_virq_cpending(virq);
		
		if (virq->hwintid)
			gic_clear_pending(virq->hwintid);	
	}
	
	return;
}

static uint32_t icpendr_read(struct lk_vcpu *vcpu, int reg_index)
{
	LTRACEF("ispendr read inedx %u\n", reg_index);
	return ispendr_read(vcpu, reg_index);
}

/* 32bit read  */
static uint32_t ipriority_read(struct lk_vcpu *vcpu, int index, int len)
{
	uint32_t val = 0;
	int irq, i;
	struct vgic_irq *virq = NULL;

	if (len != 1 && len != 4)
		return 0;

	for (i = 0; i < len; i++) {
		irq = index +i;
		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq == NULL)
			continue;
		
		val = val| (virq->priority << (8 * i));
	}

	LTRACEF("ipriority read inedx %u value %x len %d\n", index, val, len);
	return val;
}

static void ipriority_write(struct lk_vcpu *vcpu, int index, uint32_t value, int len)
{
	int pribits = 0;
	int irq;
	struct vgic_irq *virq = NULL;

	LTRACEF("ipriority write  inedx %u value %x len %d\n", index, value, len);
	for (irq = index; irq < index + len; irq++) {
		uint8_t priority;

		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq == NULL) {
			value = value >> 8;
			continue;
		}

		priority = (uint8_t)(value & (uint32_t)(((1 << pribits) - 1) << (8 - pribits)));
		virq->priority = priority;

	}

	return;
}

static uint32_t icfgr_read(struct lk_vcpu *vcpu, int index)
{
	int irq;
	int i;
	struct vgic_irq *virq = NULL;
	uint32_t val = 0;

	for (i = 0; i < 16; i++) {

		irq = index + i;
		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq == NULL)
			continue;

		if (TYPE_EDGE == virq->type)
			val = val | 1 << (2 * i + 1);
	}

	LTRACEF("icfgr read inedx %u value %x\n", index, val);
	return val;
}

static void icfgr_write(struct lk_vcpu *vcpu, int index, uint32_t value)
{
	int irq;
	int i;
	struct vgic_irq *virq = NULL;

	LTRACEF("icfgr write %u value %x\n", index, value);
	for (i = 0; i < 16; i++) {
		irq = index + i;
		virq = get_virq(vcpu->vm, vcpu, irq);
		if (virq == NULL)
			continue;

		if (value & (1 << (2 * i + 1)))
			virq->type = (uint8_t)TYPE_EDGE;
		else
			virq->type = (uint8_t)TYPE_LEVEL;	
	}

	return;
}

static uint32_t irouter_read(struct lk_vcpu *vcpu, int index)
{
	struct vgic_irq *virq = NULL;

	virq = get_virq(vcpu->vm, vcpu, index);
	if (virq == NULL) 
		return 0;

	LTRACEF("iroute read inedx %d value %llx\n", index, virq->iroute);
	return virq->iroute;
}

/* if there are some interrupts on this old_vcpu, how to transfer them  to the new vcpu  */
static void irouter_write(struct lk_vcpu *vcpu, int index, uint64_t value)
{
	uint32_t vcpu_id;
	struct vgic_irq *virq = NULL;
	struct lk_vcpu *n_vcpu = NULL;

	LTRACEF("iroute write inedx %d value %llx\n", index, value);
	virq = get_virq(vcpu->vm, vcpu, index);
	if (virq == NULL) 
		return;

	if (virq->iroute == value)
		return;


	/* for vcpu affinity, currently only support aff0 & aff1  */
	/* if dest  is the cluster, then it will be vcpu 0  */
	if (value & (1 << 31))
		vcpu_id = 0;
	else 
		vcpu_id = mpidr_to_cpu_id(value);

	n_vcpu = hv_get_vcpu(vcpu->vm , vcpu_id);
	if (n_vcpu == NULL)
		return;
	
	if (n_vcpu != virq->vcpu) {
		/* vcpu is chaged, if virq is pendind, 
		 * 	need to migrate it to new cpu  */
		if (0 > hv_migrate_virq(virq->vcpu, n_vcpu, virq))
			virq->iroute = value;
	}
}

static int dist_mmio_read(struct lk_vcpu *vcpu, paddr_t gpa, int len, uint64_t *data, void *arg)
{
	paddr_t base;
	uint32_t off = 0;
	struct vgic_dist *vdist = (struct vgic_dist *)arg;
	int index;
	uint32_t value;

	base = vdist->dist_base;
	off = gpa - base;
	LTRACEF("dist mmio read off %x\n", off);
	switch (off) {
		case GICD_CTLR:
			value = vdist->gicd_ctrl;
			break;

		case GICD_TYPER:
			value = vdist->gicd_typer;
			break;

		case GICD_IIDR:
			value = vdist->gicd_iidr;
			break;

		case GICD_TYPER2:
			value = vdist->gicd_typer2;
			break;

		case GICD_IGROUPR:
			index = get_irq_no(off, GICD_IGROUP_BASE, 5);
			value = igroupr_read(vcpu, index);
			break;

		case GICD_ISENABLER:
			index = get_irq_no(off, GICD_ISENAB_BASE, 5);
			value = isenabler_read(vcpu, index);
			break;
		
		case GICD_ICENABLER:
			index = get_irq_no(off, GICD_ICENAB_BASE, 5);
			value = icenabler_read(vcpu, index);
			break;

		case GICD_ISPENDR:
			index = get_irq_no(off, GICD_ISPEND_BASE, 5);
			value = ispendr_read(vcpu, index);
			break;

		case GICD_ICPENDR:
			index = get_irq_no(off, GICD_ICPEND_BASE, 5);
			value = ispendr_read(vcpu, index);
			break;

		case GICD_IPRIORITYR:
			index = get_irq_no(off, GICD_ICPEND_BASE, 2);
			value = ipriority_read(vcpu, index, len);
			break;

		case GICD_ICFGR:
			index = get_irq_no(off, GICD_ICFGR_BASE, 4);
			value = icfgr_read(vcpu, index);
			break;

		case GICD_IROUTER:
			index = off - GICD_IROUTER_BASE;
		        index = index >> 3;	
			value = irouter_read(vcpu, index);
			break;

		default:
		       value = 0;

	}

	/* only support 4/1  */	
	LTRACEF("write to  %p\n", data);
	if (len == sizeof(uint32_t)) { /* word  */
		*((uint32_t *)data) = (uint32_t)(value);
		return 1;
	} else if (len == 1) {              /* byte */
		*((uint8_t *)data) = (uint8_t)(value);
		return 1;
	} else
		return 1;
	
	return 0;
}

static int dist_mmio_write(struct lk_vcpu *vcpu, paddr_t gpa, int len, uint64_t data, void *arg)
{
	paddr_t base;
	uint32_t off = 0;
	struct vgic_dist *vdist = (struct vgic_dist *)arg;
	int index;

	base = vdist->dist_base;
	off = gpa - base;
	LTRACEF("dist mmio write off %x gpa %lx base %lx\n", off, gpa, base);
	switch (off) {
		case GICD_CTLR:
			vdist->gicd_ctrl = (uint32_t)data & (~GICD_CTRL_RWP);
			break;

		/* those are readonly registers */
		case GICD_TYPER:
		case GICD_IIDR:
		case GICD_TYPER2:
		case GICD_STATUSR:
			data = 0;
			break;
		
		case GICD_IGROUPR:
			index = get_irq_no(off, GICD_IGROUP_BASE, 5);
			igroupr_write(vcpu, index, (uint32_t)(data));
		        break;	

		case GICD_ISENABLER:
			index = get_irq_no(off, GICD_ISENAB_BASE, 5);
			isenabler_write(vcpu, index, (uint32_t)(data));
			break;
		
		case GICD_ICENABLER:
			index = get_irq_no(off, GICD_ICENAB_BASE, 5);
			icenabler_write(vcpu, index, (uint32_t)(data));
			break;
		
		case GICD_ISPENDR:
			index = get_irq_no(off, GICD_ISPEND_BASE, 5);
			ispendr_write(vcpu, index, (uint32_t)(data));
			break;

		case GICD_ICPENDR:
			index = get_irq_no(off,  GICD_ICPEND_BASE, 5);
			icpendr_write(vcpu, index, (uint32_t)(data));
			break;

		case GICD_IPRIORITYR:
			index = get_irq_no(off, GICD_IPPRI_BASE, 2);
			ipriority_write(vcpu, index, (uint32_t)(data), len);
			break;
		
		case GICD_ICFGR:	
			index = get_irq_no(off, GICD_ICFGR_BASE, 4);
			icfgr_write(vcpu, index, (uint32_t)(data));
			break;

		case GICD_IROUTER:
			index = (off - GICD_IROUTER_BASE) >> 3;
			irouter_write(vcpu, index, data);
			break;
			
		/* case GICD_ITARGETSR */
		/* GICD_ICFG */
		case GICD_ISACTIVER:
		default:
			data = 0;
			break;
	}
	return 1;
}

static struct mmio_ops dist_mops =  {
        .read = dist_mmio_read,
        .write = dist_mmio_write,
};


union gicr_iidr {
	uint64_t value;
	struct {
		uint32_t plpis:1;
		uint32_t vlpis:1;
		uint32_t dirty:1;
		uint32_t directlpi:1;
		uint32_t last:1;
		uint32_t dpgs:1;
		uint32_t mpam:1;
		uint32_t vpeid:1;
		uint32_t pn:16;
		uint32_t commonlpiaff:2;
		uint32_t vsgi:1;
		uint32_t ppinum:5;
		uint32_t affinity;
	};
};

/*
 * does not support lpi
 * max ppi is 31
 * */
uint64_t gicr_iidr_read(struct lk_vcpu *vcpu)
{
	struct lk_vm *vm;
	union gicr_iidr iidr;

	iidr.value = 0;
	iidr.affinity = cpu_id_to_mpidr(vcpu->id);
	iidr.ppinum = 0;
	iidr.pn = vcpu->id;

	/* check wether it is the last one   */
	vm = vcpu->vm;
	if ((int)vcpu->id == vm->vcpu_num - 1)
		iidr.last = 1;

	LTRACEF("gicr iidr read %llx\n", iidr.value);
	return iidr.value;
}

/*
 * redistributor has 2 map: rd_base sgi_base
 * */
static int redist_rd_mmio_read(struct lk_vcpu *vcpu, int len, uint64_t *data, uint32_t off)
{
	uint64_t value;

	LTRACEF("redist mmio read off %x\n", off);
	switch (off) {
		case GICR_IIDR:
			value = read_gicr_iidr();
			break;

		case GICR_TYPER:
			value = gicr_iidr_read(vcpu);
			break;

		default:
			value = 0;      /*read as 0 */
			break;	
	}

	if (len == sizeof(uint32_t)) {
		*((uint32_t *)data) = (uint32_t)(value);
		return 1;
	} else if (len == sizeof(uint64_t)) {
		*data = value;
		return 1;
	} else 
		return 0;
}

static int redist_rd_mmio_write(struct lk_vcpu *vcpu, int len, uint64_t data, uint32_t off)
{
	LTRACEF("redist mmio write off %x\n", off);
	switch (off) {
		/* ignore all  */
		default:
			break;	
	}

	return 1;
}

static int redist_sgi_mmio_read(struct lk_vcpu *vcpu,  int len, uint64_t *data, uint32_t off)
{
	uint64_t value;

	LTRACEF("sgi mmio read off %x\n", off);
	switch (off) {
		case GICR_IGROUPR0:
			value = igroupr_read(vcpu, 0);
			break;
	
		case GICR_ISENABLER0:
			value = isenabler_read(vcpu, 0);
			break;
		
		case GICR_ICENABLER0:
			value = icenabler_read(vcpu, 0); 
			break;

		case GICR_ISPENDR0:
			value = ispendr_read(vcpu, 0);	
			break;

		case GICR_ICPENDR0:
			value = icpendr_read(vcpu, 0); 
			break;

		case GICR_IPRIORITYR:
			value = ipriority_read(vcpu, 0, len);
			break;

		case GICR_ICFGR0:
			value = icfgr_read(vcpu, 0);
			break;

		case GICR_ICFGR1:
			value = icfgr_read(vcpu, 16);
			break;

		default:
			value = 0;
			break;
	}

	/* only support 4/8/1  */	
	if (len == sizeof(uint32_t)) /* word  */
		*((uint32_t *)data) = (uint32_t)(value);
	else if (len == 1)               /* byte */
		*((uint8_t *)data) = (uint8_t)(value);
	else if (len == sizeof(uint64_t))
		*data = value;
	else
		return 0;

	return 1;
}

static int redist_sgi_mmio_write(struct lk_vcpu *vcpu, int len, uint64_t data, uint32_t off)
{
	LTRACEF("sgi mmio write off %x data %llx\n", off, data);
	switch (off) {
		case GICR_IGROUPR0:
			igroupr_write(vcpu, 0, data);
			break;
	
		case GICR_ISENABLER0:
			isenabler_write(vcpu, 0, data);
			break;
		
		case GICR_ICENABLER0:
			icenabler_write(vcpu, 0, data);
			break;

		case GICR_ISPENDR0:
			ispendr_write(vcpu, 0, data);
			break;

		case GICR_ICPENDR0:
			icpendr_write(vcpu, 0, data);
			break;

		case GICR_IPRIORITYR:
			ipriority_write(vcpu, 0, data, len);
			break;

		case GICR_ICFGR0:
			icfgr_write(vcpu, 0, data);
			break;

		case GICR_ICFGR1:
			icfgr_write(vcpu, 16, data);
			break;

		default:
			break;
	}

	return 1;
}

/*  
 *
 * @vcpu is current cpu, but is may be not same with the vcpu which gpa loacated at 
 * 
 *  */
static int redist_mmio_read(struct lk_vcpu *vcpu, paddr_t gpa, int len, uint64_t *data, void *arg)
{
	int ret;
	uint32_t off = 0;
	struct vgic_redist *vdist = NULL;

	vcpu = (struct lk_vcpu *)arg;
	vdist = &(vcpu->arch_vcpu.vgic);	
	off = gpa - vdist->rd_base;
	if (off < GICR_FRAME) {
		ret = redist_rd_mmio_read(vcpu, len, data, off);
	} else {
		off = off - GICR_FRAME;
 		ret = redist_sgi_mmio_read(vcpu, len, data, off);		
	}

	return ret;
}

static int redist_mmio_write(struct lk_vcpu *vcpu, paddr_t gpa, int len, uint64_t data, void *arg)
{
	int ret;
	uint32_t off = 0;
	struct vgic_redist *vdist = NULL;

	vcpu = (struct lk_vcpu *)arg;
	vdist = &(vcpu->arch_vcpu.vgic);	

	off = gpa - vdist->rd_base;
	if (off < GICR_FRAME) {
		ret = redist_rd_mmio_write(vcpu, len, data, off);
	} else {
		off = off - GICR_FRAME;
 		ret = redist_sgi_mmio_write(vcpu, len, data, off);		
	}

	return ret;
	
}

static struct mmio_ops redist_mops = {
        .read = redist_mmio_read,
        .write = redist_mmio_write,
};

//int hv_mmio_register(struct lk_vm *vm, paddr_t addr, uint64_t len, struct mmio_ops *mops, void *args);
int hv_dist_mmio_init(struct lk_vm *vm)
{
	int ret;
	struct vgic_dist *dist = NULL;
	
	dist = hv_get_dist(vm);
	LTRACEF("register dist base is %lx len %u\n", dist->dist_base, dist->dist_len);
	ret = hv_mmio_register(vm, dist->dist_base, GICR_FRAME * 2, &dist_mops, dist);
	return ret;
}

int hv_redist_mmio_init(struct lk_vcpu *vcpu)
{
	int ret;
	struct vgic_redist *redist = NULL;
	
	redist = &(vcpu->arch_vcpu.vgic);	
	LTRACEF("register redist base is %lx len %u\n", redist->rd_base, redist->rd_len);
	ret = hv_mmio_register(vcpu->vm, redist->rd_base, redist->rd_len, &redist_mops, vcpu);
	return ret;
}

union sgi1_val {
	uint64_t val;
	struct {
		uint32_t tl:16;
		uint32_t aff1:8;
		uint32_t intid:4;
		uint32_t res0_0:4;
		uint32_t aff2:8;
		uint32_t irm:1;
		uint32_t res0:3;
		uint32_t rs:4;
		uint32_t aff3:8;
		uint32_t res0_1:8;
	};
};

/* SGI emulations  */
/* ICC_SGI1R_EL1 emulations  */
/* GICD_TYPER.RSS = 0  */
/* mpidr only use aff0|aff1 */
void gic_sgi_emulation(uint64_t value)
{
	union sgi1_val val;
	uint32_t cpu_id;
	struct lk_vcpu *vcpu, *t_vcpu;
	struct lk_vm *vm;

	vcpu = current_vcpu();
	if (NULL == vcpu)
		return;

	LTRACEF("sgi emulation value %llx\n", value);
	vm = vcpu->vm;
	val.val = value;
	if (val.irm) {
		int i = 0;
		
		LTRACEF("sgi for all of vcpus\n");
		for (i = 0; i < vm->vcpu_num; i++) {
			t_vcpu = hv_get_vcpu(vm, i);
			if (vcpu == t_vcpu)
				continue;

			hv_inject_virq(vm, t_vcpu, val.intid);	
		}
		
	} else {
		int i = 0;
		uint64_t mpidr = val.aff1 << 8;
		uint64_t tmp;

		for (i = 0; i < 16; i++) {
			if (!(val.tl & (1 << i)))
				continue;
			tmp = mpidr|i;

			LTRACEF("sgi to vcpu %llx mpidr %llx\n", tmp, mpidr);
			cpu_id = mpidr_to_cpu_id(tmp);
			t_vcpu = hv_get_vcpu(vm, cpu_id);
		       	if (NULL == t_vcpu)
				continue;
			
			hv_inject_virq(vm, t_vcpu, val.intid);		
		}	
	}

	return;
}
