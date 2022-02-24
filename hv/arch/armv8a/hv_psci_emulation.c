/* psci emulations
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
#include <hv_vcpu.h>
#include <hv_arch_traps.h>

/* psci service type definition 
 * refer : POWER STATE COORDINATION INTERFACE   */
#define PSCI_SVR_TYPE_MASK  (uint64_t)0x0000FFFF
#define PSCI_VERSION             0x0
#define CPU_SUSPEND              0x1
#define CPU_OFF                  0x2
#define CPU_ON                   0x3
#define AFFINITY_INFO            0x4
#define MIGRATE                  0x5
#define MIGRATE_INFO_TYPE        0x6
#define MIGRATE_INFO_UP_CPU      0x7
#define SYSTEM_OFF               0x8
#define SYSTEM_RESET             0x9
#define PSCI_FEATURES            0xa
#define CPU_FREEZE               0xb
#define CPU_DEFAULT_SUSPEND      0xc
#define NODE_HW_STATE            0xd
#define SYSTEM_SUSPEND           0xe
#define PSCI_SET_SUSPEND_MODE    0xf
#define PSCI_STAT_RESIDENCY      0x10
#define PSCI_STAT_COUNT          0x11

#define PSCI_VER_MAJOR           1
#define PSCI_VER_MINOR           0
#define PSCI_VER_SHIFT           16

#define PSCI_STATE_TYPE          (1 << 16)   /* original format */

/* suspend current vcpu 
 * the suspended cpu will be wakeuped by an event */
/* power_state format is Original format  */
int32_t psci_vcpu_suspend(uint32_t power_state,  uint64_t entry, uint64_t context_id)
{
	int ret = 0;
	struct lk_vcpu *vcpu = NULL;

	vcpu = current_vcpu();
	if (power_state & PSCI_STATE_TYPE) {
		ret = hv_vcpu_block(vcpu);
		VCPU_FLAGS_SET(vcpu, VCPU_FLAGS_EVENT);
	} else
		ret = hv_vcpu_poweroff(vcpu);	

	return (ret >= 0) ? SMC_SUCCESS : SMC_DENIED;
}

int32_t psci_vcpu_off(void)
{
	int ret;
	struct lk_vcpu *vcpu = NULL;

	vcpu = current_vcpu();
	ret = hv_vcpu_poweroff(vcpu);
	return (ret >= 0) ? SMC_SUCCESS : SMC_DENIED;
}

/* vcpu on */
int32_t psci_vcpu_on(uint64_t target_vcpu, uint64_t entry, uint64_t ctx_id)
{
	int ret;
	struct lk_vcpu *vcpu = NULL;
	struct lk_vcpu *tgt_vcpu = NULL;
	struct lk_vm   *vm = NULL;
	int tgt_id;

	vcpu = current_vcpu();
	tgt_id = mpidr_to_cpu_id(target_vcpu);
	vm = vcpu->vm;

	if (tgt_id >= vm->vcpu_num)
		return SMC_INVALID_PARAMETERS;
	
	tgt_vcpu = vm->vcpu[tgt_id];
	tgt_vcpu->entry = (void *)entry;
	tgt_vcpu->args = (void *)ctx_id;

	ret = hv_vcpu_start(tgt_vcpu);
	if (ret == 0)
		return SMC_SUCCESS;
	else if (ret == ERR_BUSY)
		return SMC_ALREADY_ON;
	else 
		return SMC_DENIED; 
}

/* psci system off  */
int32_t psci_sys_off(void)
{
	int ret;
	struct lk_vcpu *vcpu = NULL;
	struct lk_vm   *vm = NULL;

	vcpu = current_vcpu();
	vm = vcpu->vm;
	ret = lkhv_shutdown_vm(vm);   
	
	return (ret >= 0) ? SMC_SUCCESS : SMC_DENIED;	 /* will never return here */
}

/* system reset  */
int32_t psci_sys_reset(void)
{
	int ret;
	struct lk_vcpu *vcpu = NULL;
	struct lk_vm   *vm = NULL;

	vcpu = current_vcpu();
	vm = vcpu->vm;
	ret = lkhv_shutdown_vm(vm);
	if (0 != ret)
		return SMC_DENIED;

	ret = lkhv_vm_start(vm);		
	return (ret >= 0) ? SMC_SUCCESS : SMC_DENIED;	
}

/* psci features 
 * Bits [31:2] : Reserved, must be zero
Bits[1:1] : Parameter Format bit
0 if the implementation uses Original Format
(PSCI0.2) for the power_state
parameter
1 if the implementation uses the new
extended StateID Format for the
power_state parameter
Bits[0:0]: OS Initiated Mode
0 if the platform does not support OS
Initiated mode
1 if the platform supports OS Initiated mode */
int32_t psci_features(uint32_t fun_id)
{
	uint32_t status = 0;
	uint32_t svr = 0;

	svr = fun_id & PSCI_SVR_TYPE_MASK;
	switch (svr) {
		case PSCI_VERSION:
		case CPU_SUSPEND:
		case CPU_OFF:
		case CPU_ON:
		case SYSTEM_OFF:
		case SYSTEM_RESET:
		case PSCI_FEATURES:
			status = 0; /* original format, does not support OS Initiated mode */ 
			break;

		case AFFINITY_INFO:
		case MIGRATE:
		case MIGRATE_INFO_TYPE:
		case MIGRATE_INFO_UP_CPU:
		case CPU_DEFAULT_SUSPEND:
		case NODE_HW_STATE:
		case PSCI_SET_SUSPEND_MODE:
		case PSCI_STAT_RESIDENCY:
		case PSCI_STAT_COUNT:
		case CPU_FREEZE:
		case SYSTEM_SUSPEND:
		default:	
			status = SMC_NOT_SUPPORTED;	
			break;	
	}

	return status;
}

int32_t hv_psci_call(struct arm64_iframe_long *iframe)
{
	uint64_t x0 = iframe->r[0];
	int32_t svr = x0 & PSCI_SVR_TYPE_MASK;
	int32_t status = 0;

	switch (svr) {
		case PSCI_VERSION:
		       status = PSCI_VER_MAJOR << PSCI_VER_SHIFT | PSCI_VER_MINOR;
		       break;
		
		case CPU_SUSPEND:
		       status = psci_vcpu_suspend(iframe->r[1], iframe->r[2], iframe->r[3]);
		       break;
		
		case CPU_OFF:
		       status = psci_vcpu_off();
			break;
		
		case CPU_ON:
			status = psci_vcpu_on(iframe->r[1], iframe->r[2], iframe->r[3]);
			break;

		case AFFINITY_INFO:
		case MIGRATE:
		case MIGRATE_INFO_TYPE:
		case MIGRATE_INFO_UP_CPU:
		case CPU_DEFAULT_SUSPEND:
		case NODE_HW_STATE:
		case PSCI_SET_SUSPEND_MODE:
		case PSCI_STAT_RESIDENCY:
		case PSCI_STAT_COUNT:
		case CPU_FREEZE:
		case SYSTEM_SUSPEND:	
			status = SMC_NOT_SUPPORTED;	
			break;
			
		case SYSTEM_OFF:
			status = psci_sys_off();
			break;
		
		case SYSTEM_RESET:
			status = psci_sys_reset();
			break;

		case PSCI_FEATURES:
			status = psci_features(iframe->r[1]);
			break;	
	}	
	return status;
}
