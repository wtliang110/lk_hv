/* timer virtulization  
 * @author pwang
 * @mail wtliang110@aliyun.com
 *
 * This program is free software; you can redistribute it and/or modify
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
#include <sys/types.h>
#include <arch/arm64.h>
#include <string.h>
#include <hv_arch_vcpu.h>
#include <hv_vcpu.h>
#include <dev/interrupt/arm_gic.h>
#include <hv_vgic.h>
#include <kernel/spinlock.h>
#include <lk/list.h>
#include <lk/trace.h>
#include <hv_vtimer.h>
#include <arch/arm64.h>
#include <hv_arch_irq.h>

#define LOCAL_TRACE 1

/* 
 * design:
 * 1. CNTPCT_EL0 is readonly, let the program to read it. but not to programe physical counter
 * 2. why mentor permit access physical timer ? 
 * 3. guest user vtimer_el1, can access phycial counter
 * 4. guest can not use ptimer_el1, if guest access it, will injection a exception to guest
 * 5. guest can use event stream.
 * 6. hypervisor use ptimer_el1
 * 7. set a timer before vcpu before become idle(into a low power) if vtimer is not idle, trigger a timer irq when vcpu is scheduled
 * 8. vcpu sleep operation
 * 9. how event stream works ????????????? 
 * 10. vtimer for vcpu migration between cpus. 
 *
 * */

/* irq for vtimer_el1  */
/* ppi irq usage is defined by the Server Base System Architecture (SBSA) */
#define IRQ_VTIMER_EL1 27
#define VCPU_TIMER(vcpu) &((vcpu)->arch_vcpu.vtimer)
#define HV_VTIMER_ENABLE      (1 << 0)
#define HV_VTIMER_IMASK       (1 << 1)
#define HV_VTIMER_ISTATUS     (1 << 2)

void hv_vtimer_context_save(struct lk_vcpu *vcpu)
{
	uint64_t val = 0;
	struct arch_vtimer *vt = VCPU_TIMER(vcpu);

	vt->cntvctl_el1 = ARM64_READ_SYSREG(cntv_ctl_el0);

	/* disable el0 */
	val = (vt->cntvctl_el1 & ~HV_VTIMER_ENABLE);
	ARM64_WRITE_SYSREG(cntv_ctl_el0, val);

	vt->cntpct = ARM64_READ_SYSREG(cntpct_el0); 
	vt->cntvcval_el1 = ARM64_READ_SYSREG(cntv_cval_el0);
	vt->cntvtval_el1 = ARM64_READ_SYSREG(cntv_tval_el0);

	/* do not set a timer, reason:
	 * 1. when the timer irq comming, vcpu  will not handle it if it was not scheduled
	 * 2. when the vtimer is restore, it would  tigger a virtual interrupt if the vtimer was arrived  */
	return;
}

void hv_vtimer_context_restore(struct lk_vcpu *vcpu)
{
	struct arch_vtimer *vt = VCPU_TIMER(vcpu);

	//ARM64_WRITE_SYSREG(cntpct_el0, vt->cntpct); 
	ARM64_WRITE_SYSREG(cntv_ctl_el0, vt->cntvctl_el1); 
	ARM64_WRITE_SYSREG(cntv_cval_el0, vt->cntvcval_el1);
	ARM64_WRITE_SYSREG(cntv_tval_el0, vt->cntvtval_el1);
	ARM64_WRITE_SYSREG(cntvoff_el2, vt->cntvoff_el2);
	//printf("-------------cntvoff_el2 is %lx\n", ARM64_READ_SYSREG(cntvoff_el2));
	return;
}

void hv_vcpu_vtimer_init(struct lk_vcpu *vcpu)
{
	struct arch_vtimer *vt = VCPU_TIMER(vcpu);
	
	memset(vt, 0, sizeof(*(vt)));
	vt->cntvoff_el2 = ARM64_READ_SYSREG(cntpct_el0);
	//printf(" cntvoff_el2 is %lx\n", vt->cntvoff_el2);
	//vt->cntvctl_el1 = HV_VTIMER_ENABLE;
}

/* vtimer handler  */
static enum handler_return vtimer_irq_handler(void *arg)
{
	int ret = 0;
	struct lk_vcpu *vcpu = NULL;

	vcpu = current_vcpu();
	if (vcpu == NULL)
		return 0;

	LTRACEF("virtual timer irq is coming\n");
	ret = hv_inject_virq(vcpu->vm, vcpu, IRQ_VTIMER_EL1);
	if (ret != 0)
		LTRACEF("failed to inject virq timer\n");

	return 0;
}

union cnthctl {
	uint64_t val;
	struct {
		uint32_t el1pcten:1; /* Traps EL0 and EL1 accesses to the EL1 physical counter register to EL2  */
		uint32_t el1pcen:1;  /* Traps EL0 and EL1 accesses to the EL1 physical timer registers to EL2 */ 
		uint32_t evnten:1;   /* Enables the generation of an event stream from the counter register CNTPCT_EL0  */
		uint32_t evntdir:1;  /* Controls which transition of the counter register CNTPCT_EL0 trigger bit, defined by EVNTI */
		uint32_t evnti:4;    /* Selects which bit of the counter register CNTPCT_EL0 is the trigger for the event stream generated from that counter */
		uint32_t res0_0:4;
		uint32_t ecv:1;      /* Enables the Enhanced Counter Virtualization functionality registers  */
		uint32_t el1tvt:1;   /* Traps EL0 and EL1 accesses to the EL1 virtual timer registers to EL2 */
		uint32_t el1tvct:1;  /* Traps EL0 and EL1 accesses to the EL1 virtual counter registers to EL2 */
		uint32_t el1nvpct:1; /* Traps EL1 accesses to the specified EL1 physical timer registers using the EL02 descriptors to EL2  */
		uint32_t el1nvvct:1; /* Traps EL1 accesses to the specified EL1 virtual timer registers using the EL02 descriptors to EL2 */
		uint32_t evntis:1;
		uint32_t res0_1:14;  /* res  */
		uint32_t res0_2;
	};	
};

/*  will be called on each cpu core */
int hv_vtimer_init(void)
{
	uint64_t val = 0;
	union cnthctl cnthctl;
	
	LTRACEF("virtual timer init \n");

	/* vtimer init  : CNTHCTL_EL2 */
	cnthctl.val = 0;
	cnthctl.evnten = 1;
	cnthctl.el1pcten = 1;
	ARM64_WRITE_SYSREG(cnthctl_el2, cnthctl.val);

	/* first disable vtimer  */
	val = ARM64_READ_SYSREG(cntv_ctl_el0);
	val = (val & ~HV_VTIMER_ENABLE);
	ARM64_WRITE_SYSREG(cntv_ctl_el0, val);

	/* register irq handler */
	LTRACEF("register handler for irq %u\n \n", IRQ_VTIMER_EL1);
	register_int_handler(IRQ_VTIMER_EL1, vtimer_irq_handler, (void *)INT_IRQ);
	return 0;
}
