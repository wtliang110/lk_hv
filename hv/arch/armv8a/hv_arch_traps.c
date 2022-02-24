/* hypervisor traps and emulations
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
#include <hv_vm.h>
#include <hv_mem.h>
#include <stdio.h>
#include <kernel/vm.h>
#include <lk/trace.h>
#include <lk/bits.h>
#include <arch/arch_ops.h>
#include <arch/arm64.h>
#include <hv_arch_traps.h>
#include <hv_vgic.h>
#include <hv_mmio.h>
#include <vgic_mmio.h>
#include <arch/arm64/hv_reg.h>

#define LOCAL_TRACE 0

/* smc managment: pm  */
void hv_traps_smc(struct arm64_iframe_long *iframe)
{
	int32_t status = 0;
	uint64_t x0 = iframe->r[0];
	uint32_t type = SMC_TYPE_MASK & x0; 

	switch (type) {
		case SMC_TYPE_PSCI_CALL:
			status = hv_psci_call(iframe);
			break;
		default:
			/* others currently does not support */
			status  = SMC_NOT_SUPPORTED;
			break;
	}	
	
	iframe->r[0] = status;		
	return;
}

/* TODO: hypervisor call */
static int32_t hyper_svr_call(struct arm64_iframe_long *iframe)
{
	return 0;
}

void hv_traps_hypercall(struct arm64_iframe_long *iframe)
{
	int32_t status = 0;
	uint64_t x0 = iframe->r[0];
	uint32_t type = SMC_TYPE_MASK & x0; 

	switch (type) {
		case SMC_TYPE_PSCI_CALL:
			status = hv_psci_call(iframe);
			break;

		case SMC_HYPER_CALL:
		        status = hyper_svr_call(iframe);
			break;

		default:
			/* others currently does not support */
			status  = SMC_NOT_SUPPORTED;
			break;
	}	
	
	iframe->r[0] = status;		
	return;
}

/* data abort from lower level */
union hda_iss {
	uint32_t bits;
	struct {
		uint32_t dfsc:6;       /* Data Fault Status Code */
		uint32_t wnr:1;        /* Write not Read */
		uint32_t s1ptw:1;      /* indicates whether the fault was a stage 2 fault 
					  on an access made for a stage  translation table walk:  */
		uint32_t cm:1;         /* cache maintenance  */
		uint32_t ea:1;         /* External abort type */
		uint32_t fnv:1;        /* FAR not Valid  */
		uint32_t set:2;        /* Synchronous Error Type  */
		uint32_t vncr:1;       /* Indicates that the fault came from use of VNCR_EL2 register by EL1 code */
		uint32_t ar:1;         /* Acquire/Release  */
		uint32_t sf:1;         /* Width of the register accessed by the instruction is Sixty-Four  */
		uint32_t srt:5;        /* Syndrome Register Transfer */
		uint32_t sse:1;        /* Syndrome Sign Extend */
		uint32_t sas:2;        /* Syndrome Access Size  */
		uint32_t isv:1;        /* Instruction Syndrome Valid */
	};
};

#define HPFAR_MASK (uint64_t)(~0xf)
#define FSC_FLT_TRANS  (0x04)
#define FSC_FLT_ACCESS (0x08)
#define FSC_FLT_PERM   (0x0c)
#define FSC_SEA        (0x10) /* Synchronous External Abort */
#define FSC_SPE        (0x18) /* Memory Access Synchronous Parity Error */
#define FSC_APE        (0x11) /* Memory Access Asynchronous Parity Error */
#define FSC_SEATT      (0x14) /* Sync. Ext. Abort Translation Table */
#define FSC_SPETT      (0x1c) /* Sync. Parity. Error Translation Table */
#define FSC_AF         (0x21) /* Alignment Fault */
#define FSC_DE         (0x22) /* Debug Event */
#define FSC_LKD        (0x34) /* Lockdown Abort */
#define FSC_CPR        (0x3a) /* Coprocossor Abort */

int hv_data_abort(struct arm64_iframe_long *iframe, uint32_t esr)
{
	union hda_iss da_iss;
	//uint32_t ec = BITS_SHIFT(esr, 31, 26);
	//uint32_t il = BIT(esr, 25);
	uint64_t gva, ipa;
	uint64_t data;
	int data_len, is_write, reg_num, is_signed;

	da_iss.bits = BITS(esr, 24, 0);

	/* following case, return:
	 * not a stage 2 fault
	 * syndrome is invalid
	 * external data about  */
	if (!da_iss.isv)
		return 0;

	gva = ARM64_READ_SYSREG(FAR_EL2);
	gva = gva & (PAGE_SIZE - 1);
	ipa = ARM64_READ_SYSREG(HPFAR_EL2);
	ipa = (ipa & HPFAR_MASK) << (12 - 4);              /* there is no document about this.   */
	ipa = ipa | gva;

	is_write = da_iss.wnr;
	data_len = 1 << da_iss.sas;
	reg_num = da_iss.srt;
	is_signed = da_iss.sse;
	if (is_write)
		data = (reg_num == 31) ? 0 : iframe->r[reg_num];
	else
		data = (reg_num < 30) ?  (uint64_t)(iframe->r + reg_num) : 0;


	if (da_iss.dfsc & FSC_FLT_TRANS) {
		return hv_mmio_handler(ipa, data_len, is_write, data, is_signed);
	} else {
		/* TODO others :  permission check and others   */
	}

	return 0;
}

/* instruction abort from lower level  */
static int hv_inst_abort(struct arm64_iframe_long *iframe, uint32_t esr)
{
	return 0;
}

union exce_esr {
	uint32_t val;
	struct {
		uint32_t direction:1;
		uint32_t crm:4;
		uint32_t rt:5;             /* The Rt value from the issued instruction, the general-purpose register used for the transfer */
		uint32_t crn:4;
		uint32_t op1:3;
		uint32_t op2:3;
		uint32_t op0:2;
		uint32_t res0:3;
		uint32_t il:1;
		uint32_t ec:6;
	};
};

/* the code fore sgi1r sgi0r asgi1r */
#define IO_REG_MASK                   0b00000000001111111111110000011110
#define SGI1R_EL1_VALUE               0b00000000001110100011000000010110
#define SGI0R_EL1_VALUE               0b00000000001111100011000000010110
#define ASGI1R_EL1_VALUE              0b00000000001111000011000000010110

/* MSR/MRS/system instruction emulation */
static int hv_io_emulation(struct arm64_iframe_long *iframe, uint32_t esr)
{
	int ret;
	int is_r;
	uint64_t value = 0;
	uint32_t mask_val;
	union exce_esr e_esr;
	
	e_esr.val = esr;
	is_r = e_esr.direction;
	if (!is_r)
		value = iframe->r[e_esr.rt];

	LTRACEF("hv io emulatins %x\n", esr);
	mask_val = esr & IO_REG_MASK;
	switch (mask_val) {
		case SGI1R_EL1_VALUE:
			gic_sgi_emulation(value);
			ret = 1;
			break;
		
		default:
			ret = 0;	
	}
			
	return ret;
}

/* about el definition: C5.2.18 SPSR_EL2, 
 * Saved Program Status Register (EL2)  */
#if 0
/* return error exception to el1, 
 * currently is only support exception from aarch64  */
void hv_excep_to_el1(struct arm64_iframe_long *iframe)
{
	uint64_t far;
	uint64_t hcr;
	
	far = ARM64_READ_SYSREG(FAR_EL2);
	hcr = ARM64_READ_SYSREG(HCR_EL2);
	hcr = hcr |HCR_VSE; 
	
	/* set exception context  */
	ARM64_WRITE_SYSREG(HCR_EL2, hcr);
	ARM64_WRITE_SYSREG(FAR_EL1, far);

	return;	
}
#else

#define PSR_FIQ_MASK    (1<<6)        /* Fast Interrupt mask */
#define PSR_IRQ_MASK    (1<<7)        /* Interrupt mask */
#define PSR_ABT_MASK    (1<<8)        /* Asynchronous Abort mask */

#define SPSR_EL0  0b0000
#define SPSR_EL1t 0b0100
#define SPSR_EL1h 0b0101

static int is_el_el0(uint32_t el)
{
	return el == SPSR_EL0;
}

static int is_el_el1(int32_t el)
{
	return (el == SPSR_EL1t) || (el == SPSR_EL1h);
}

paddr_t get_el1_excep_entry(struct arm64_iframe_long *iframe)
{
	uint32_t v_off = 0;
	paddr_t vbar_el1 = ARM64_READ_SYSREG(vbar_el1);
	uint64_t  spsr = ARM64_READ_SYSREG(spsr_el2);
	uint32_t el;

	el = spsr & 0xf;
	if (is_el_el0(el))
		v_off = 0x400;
	else if (is_el_el1(el))
		v_off = 0x200;
	else
		return -1;

	return vbar_el1 + v_off;
}

#define ESR_DABT 0
#define ESR_IABT 1
#define ESR_UNKOWN 2
#define HSR_EC_INSTR_ABORT_LOWER_EL 0x20
#define HSR_EC_INSTR_ABORT_CURR_EL  0x21
#define HSR_EC_DATA_ABORT_LOWER_EL  0x24
#define HSR_EC_DATA_ABORT_CURR_EL   0x25

uint32_t calc_esr_ec(int reason)
{
	uint32_t ec = 0;
	uint64_t  spsr = ARM64_READ_SYSREG(spsr_el2);
	uint32_t el;
	el = spsr & 0xf;

	switch (reason) {
		case ESR_DABT:
			if (is_el_el0(el))
				ec = HSR_EC_DATA_ABORT_LOWER_EL;
			else if (is_el_el1(el))
				ec = HSR_EC_DATA_ABORT_CURR_EL;	
			break;

		case ESR_IABT:
			if (is_el_el0(el))
				ec = HSR_EC_INSTR_ABORT_LOWER_EL;
			else if (is_el_el1(el))
				ec = HSR_EC_INSTR_ABORT_CURR_EL;	
			break;

		default:
			break;
	}

	return ec;
}

union esr_el1 {
	uint64_t val;
	struct {
		uint32_t iss:25;
		uint32_t il:1;
		uint32_t ec:6;
		uint32_t iss2:5;
		uint32_t res0:27;
	};
};

void hv_excep_to_el1(struct arm64_iframe_long *iframe,  int reason)
{
	union esr_el1 esr;
	esr.val = ARM64_READ_SYSREG(esr_el2);
	

	/* spsr_el1 */
	ARM64_WRITE_SYSREG(spsr_el1, iframe->spsr);

	/* elr_el1 */
	ARM64_WRITE_SYSREG(elr_el1, iframe->elr);

	/* cpsr_el1 */
	iframe->spsr = iframe->spsr|PSR_FIQ_MASK|PSR_IRQ_MASK|PSR_ABT_MASK;
	iframe->elr = get_el1_excep_entry(iframe); 

	/* esr_el1 */
	esr.ec = calc_esr_ec(reason); 
	ARM64_WRITE_SYSREG(esr_el1, esr.val);

	return;
}

#endif 

/*  handle hypervisor related exceptions */
int hyper_sync_exception(struct arm64_iframe_long *iframe, uint32_t esr) 
{
	uint32_t ec = BITS_SHIFT(esr, 31, 26);
	uint64_t spsr = ARM64_READ_SYSREG(spsr_el2);
	uint32_t el;
	int ret;

	/* ignore exception from same exception level */
	el = spsr & (uint64_t)0xf;
	if (el == 0b1000 || el == 0b1001)
		return 0;

	hv_virq_from_lrs();
	switch (ec) {

        	/* data abort from lower level 
		 * if el2 does not handle it, inject it to el1 */
		case EXCP_EC_LDA:
			ret = hv_data_abort(iframe, esr);
			if (ret)
				iframe->elr = iframe->elr + 4;
			else {
				hv_excep_to_el1(iframe, ESR_DABT);
				ret = 1;
			}
			break;

		/* instruction from lower level */
		case EXCP_EC_LIA:
			ret = hv_inst_abort(iframe, esr);
			break;

		/*  hvc call  */
		case EXCP_EC_HVC: 
	       		hv_traps_hypercall(iframe);
			printf("hvc call interface\n");
			ret = 1;
		     	break;	

		/* smc call */
		case EXCP_EC_SMC: 
			hv_traps_smc(iframe);
			printf("smc call interface\n");
			ret = 1;
			break;

		/* Trapped MSR, MRS or System instruction execution in AArch64 state,
		 * if do not support this emulation, inject it to el1  */
		case EXCP_EC_MRS:
			ret = hv_io_emulation(iframe, esr);
			if (ret)
				iframe->elr = iframe->elr + 4;
			else {
				ret = 1;
				hv_excep_to_el1(iframe, ESR_UNKOWN);
			}
			break;

		/* Trapped WF* instruction execution */
		case EXCP_EC_WFX:
			ret = 1;
			break;
        	
		default:
			printf("unhandled synchronous exception\n");
			ret = 0;
			break;
	}
	
	hv_virq_to_lrs();

	/* if hv emuate it, need to run next   */
	return ret;

}
