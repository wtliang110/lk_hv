/* gic virtulization driver
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
#include <vgic_gic.h>
#include <hv_vgic.h>
#include <lk/trace.h>
#include <dev/interrupt/arm_gic.h>

#define ICH_VTR_EL2             S3_4_C12_C11_1
#define ICH_ELRSR_EL2           S3_4_C12_C11_5

#define LOCAL_TRACE             0
#define MAINT_INTER_NO          25

struct vgic_info {
	uint8_t lr_nr;   /* the max lr number */
	uint8_t pribits; /* priority  bits support */
};

static struct vgic_info vgic_info;
enum handler_return vgic_maintenance_inter(void *arg);

int  vgic_gic_init(void)
{
	int ret = 0;

	union ich_vtr {
		uint64_t val;
		struct {
			uint32_t listregs:5;
			uint32_t bits1:13;
			uint32_t bits2:5;
		     	uint32_t itbits:3;
			uint32_t prebits:3;
			uint32_t pribits:3;
			uint32_t bits3;	
		};
	};
	union ich_vtr ich_vtr;

	ich_vtr.val = ARM64_READ_SYSREG(ICH_VTR_EL2);
	vgic_info.lr_nr = ich_vtr.listregs;
	vgic_info.pribits = ich_vtr.pribits;
	LTRACEF("----------------------lr number is %d\n", vgic_info.lr_nr);

	/* register mantenance interrupt */
	ret = gic_set_irq(MAINT_INTER_NO, GIC_PRI_HV, 1, 1, vgic_maintenance_inter, NULL); 
	return ret;
}

/* get how many lrs we have  */
uint8_t vgic_get_lr_nr(void)
{
	return vgic_info.lr_nr;
}

uint8_t vgic_get_pribits(void)
{
	return vgic_info.pribits;
}

uint8_t vgic_get_lr(void)
{
	int i = 0;
	uint64_t elrsr;

	elrsr = ARM64_READ_SYSREG(ICH_ELRSR_EL2);
	for (i = 0; i < vgic_info.lr_nr; i++) {
		if (elrsr & 1)
			return i;
		elrsr = elrsr >> 1;
	}
	return LR_INDEX_INVALID;
}

#define ICH_LR0_EL2                            S3_4_C12_C12_0
#define ICH_LR1_EL2                            S3_4_C12_C12_1
#define ICH_LR2_EL2                            S3_4_C12_C12_2
#define ICH_LR3_EL2                            S3_4_C12_C12_3
#define ICH_LR4_EL2                            S3_4_C12_C12_4
#define ICH_LR5_EL2                            S3_4_C12_C12_5
#define ICH_LR6_EL2                            S3_4_C12_C12_6
#define ICH_LR7_EL2                            S3_4_C12_C12_7
#define ICH_LR8_EL2                            S3_4_C12_C13_0
#define ICH_LR9_EL2                            S3_4_C12_C13_1
#define ICH_LR10_EL2                           S3_4_C12_C13_2
#define ICH_LR11_EL2                           S3_4_C12_C13_3
#define ICH_LR12_EL2                           S3_4_C12_C13_4
#define ICH_LR13_EL2                           S3_4_C12_C13_5
#define ICH_LR14_EL2                           S3_4_C12_C13_6
#define ICH_LR15_EL2                           S3_4_C12_C13_7

#define WRITE1_LR(n, value)		       ARM64_WRITE_SYSREG(S3_4_C12_C12_##n, value) 
#define WRITE2_LR(n, value)                    ARM64_WRITE_SYSREG(S3_4_C12_C13_##n, value)
#define READ1_LR(n)                            ARM64_READ_SYSREG(S3_4_C12_C12_##n) 
#define READ2_LR(n)                            ARM64_READ_SYSREG(S3_4_C12_C13_##n) 

static inline void lr_write(int lr, uint64_t value)
{
	LTRACEF("write %llx to lr %d\n", value, lr);
	switch (lr) {
		case 0:
			WRITE1_LR(0, value);
			break;
		case 1:
			WRITE1_LR(1, value);
			break;
		case 2:
			WRITE1_LR(2, value);
			break;
		case 3:
			WRITE1_LR(3, value);
			break;
		case 4:
			WRITE1_LR(4, value);
			break;
		case 5:
			WRITE1_LR(5, value);
			break;
		case 6:
			WRITE1_LR(6, value);
			break;
		case 7:
			WRITE1_LR(7, value);
			break;
		case 8:
			WRITE2_LR(0, value);
			break;
		case 9:
			WRITE2_LR(1, value);
			break;
		case 10:
			WRITE2_LR(2, value);
			break;
		case 11:
			WRITE2_LR(3, value);
			break;
		case 12:
			WRITE2_LR(4, value);
			break;
		case 13:
			WRITE2_LR(5, value);
			break;
		case 14:
			WRITE2_LR(6, value);
			break;
		case 15:
			WRITE2_LR(7, value);
			break;
		default:
			break;
	}	

	return;
}

static inline uint64_t lr_read(int lr)
{
	uint64_t val = 0;

	switch (lr) {
		case 0:
			val = READ1_LR(0);
			break;
		case 1:
			val = READ1_LR(1);
			break;
		case 2:
			val = READ1_LR(2);
			break;
		case 3:
			val = READ1_LR(3);
			break;
		case 4:
			val = READ1_LR(4);
			break;
		case 5:
			val = READ1_LR(5);
			break;
		case 6:
			val = READ1_LR(6);
			break;
		case 7:
			val = READ1_LR(7);
			break;
		case 8:
			val = READ2_LR(0);
			break;
		case 9:
			val = READ2_LR(1);
			break;
		case 10:
			val = READ2_LR(2);
			break;
		case 11:
			val = READ2_LR(3);
			break;
		case 12:
			val = READ2_LR(4);
			break;
		case 13:
			val = READ2_LR(5);
			break;
		case 14:
			val = READ2_LR(6);
			break;
		case 15:
			val = READ2_LR(7);
			break;
		default:
			break;
	}	

	LTRACEF("read lr %d val %llx\n", lr, val);
	return val;
}

union lr_val {
	uint64_t val;
	struct {
		uint32_t vintid;
		uint32_t pintid:13;
		uint32_t res0:3;
		uint32_t pri:8;
		uint32_t ares0:4;
		uint32_t group:1;
		uint32_t hw:1;
		uint32_t state:2;
	};
};

/* virq is not in lrs yes, write @virq into lr  */
void vgic_write_lr(struct vgic_irq *virq)
{
	union lr_val v_lr;

	v_lr.val = 0;
	v_lr.vintid = virq->intid;
	v_lr.pintid = virq->hwintid;
	v_lr.pri = virq->priority;
	v_lr.group = virq->group;                  /* default one is 1  */
	v_lr.state = LR_ST_PENDING;

	if (virq->hwintid)
		v_lr.hw = 1;
	
	LTRACEF("write %llx to lr %d\n", v_lr.val, virq->lr);
	lr_write(virq->lr, v_lr.val);
	return;
}

/* update virq to lr active pending  */
void vgic_update_lr(struct vgic_irq *virq)
{
	union lr_val v_lr;
	
	v_lr.val = lr_read(virq->lr);
	if (v_lr.state == LR_ST_ACTIVE) {
		v_lr.state = LR_ST_ACT_PEND;
		lr_write(virq->lr, v_lr.val);
	}	
	return;
}

/* maintenance handler  */
enum handler_return vgic_maintenance_inter(void *arg)
{
	return INT_NO_RESCHEDULE; 
}

/* turn on maintenance interrupt reason is @reason  */
void vgic_maintenace_on(int reason)
{
	uint64_t ich_hcr_val = 0;

	if (reason == 0 || reason > 254)
		return;

	ich_hcr_val = ARM64_READ_SYSREG(ICH_HCR_EL2);
	ich_hcr_val = ich_hcr_val|reason;
	ARM64_WRITE_SYSREG(ICH_HCR_EL2, ich_hcr_val);
	return;
}

/* turn off maintenance interrupt, reason is @reason */
void vgic_maintenace_off(int reason)
{
	uint64_t ich_hcr_val = 0;

	if (reason == 0 || reason > 254)
		return;

	ich_hcr_val = ARM64_READ_SYSREG(ICH_HCR_EL2);
	ich_hcr_val = ich_hcr_val & (~reason);
	ARM64_WRITE_SYSREG(ICH_HCR_EL2, ich_hcr_val);

	return;
}

/* read lr value by lr index @lr  */
void vgic_read_lr(uint8_t lr, struct gic_lr *val)
{
	union lr_val l_val = {.val = lr_read(lr),};

	val->virq = l_val.vintid;
	val->priority = l_val.pintid;
	val->status = l_val.state;
	val->pirq = l_val.pintid;

	return;
}

uint64_t vgic_read_lr_reg(uint8_t lr)
{
	LTRACEF("read value from lr %d\n", lr);
	return lr_read(lr);
}

void vgic_write_lr_reg(uint8_t lr, uint64_t val)
{
	lr_write(lr, val);
}

#define ICH_AP0R0_EL2          S3_4_C12_C8_0
#define ICH_AP0R1_EL2          S3_4_C12_C8_1
#define ICH_AP0R2_EL2          S3_4_C12_C8_2
#define ICH_AP0R3_EL2          S3_4_C12_C8_3
#define ICH_AP1R0_EL2          S3_4_C12_C9_0
#define ICH_AP1R1_EL2          S3_4_C12_C9_1
#define ICH_AP1R2_EL2          S3_4_C12_C9_2
#define ICH_AP1R3_EL2          S3_4_C12_C9_3

/* read/write  ICH_AP0R<n>_EL2  */
uint64_t vgic_read_aprn_reg(uint8_t nr, uint8_t is_group0)
{
	uint64_t val;
	
	LTRACEF("read aprn %d group %d\n", nr, is_group0);
	switch (nr) {
		case 0:
			if (is_group0)
				val = ARM64_READ_SYSREG(ICH_AP0R0_EL2);
			else
				val = ARM64_READ_SYSREG(ICH_AP1R0_EL2);			
			break;
		case 1:
			if (is_group0)
				val = ARM64_READ_SYSREG(ICH_AP0R1_EL2);
			else
				val = ARM64_READ_SYSREG(ICH_AP1R1_EL2);			
			break;
		case 2:
			if (is_group0)
				val = ARM64_READ_SYSREG(ICH_AP0R2_EL2);
			else
				val = ARM64_READ_SYSREG(ICH_AP1R2_EL2);			
			break;
		case 3:
			if (is_group0)
				val = ARM64_READ_SYSREG(ICH_AP0R3_EL2);
			else
				val = ARM64_READ_SYSREG(ICH_AP1R3_EL2);			
			break;
		default:
			return 0;
	}

	return val;
}

void vgic_write_aprn_reg(uint8_t nr, uint8_t is_group0, uint64_t val)
{	
	LTRACEF("write aprn %d group %d %llx\n", nr, is_group0, val);
	switch (nr) {
		case 0:
			if (is_group0)
				ARM64_WRITE_SYSREG(ICH_AP0R0_EL2, val);
			else
				ARM64_WRITE_SYSREG(ICH_AP1R0_EL2, val);
			break;
		case 1:
			if (is_group0)
				ARM64_WRITE_SYSREG(ICH_AP0R1_EL2, val);
			else
				ARM64_WRITE_SYSREG(ICH_AP1R1_EL2, val);
			break;
		case 2:
			if (is_group0)
				ARM64_WRITE_SYSREG(ICH_AP0R2_EL2, val);
			else
				ARM64_WRITE_SYSREG(ICH_AP1R2_EL2, val);
			break;
		case 3:
			if (is_group0)
				ARM64_WRITE_SYSREG(ICH_AP0R3_EL2, val);
			else
				ARM64_WRITE_SYSREG(ICH_AP1R3_EL2, val);
			break;
		default:
			return;
	}
	return;
}
