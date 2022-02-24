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
#ifndef __HV_ARCH_TRAPS_H__
#define __HV_ARCH_TRAPS_H__

/* currently does not support aarch32 mode   */

#define EXCP_EC_WFX     0b000001    /*  Trapped WF* instruction execution  */ 
/* Access to SVE, Advanced SIMD or floating-point functionality trapped by
CPACR_EL1.FPEN, CPTR_EL2.FPEN, CPTR_EL2.TFP, or CPTR_EL3.TFP control   */
#define EXCP_EC_FP      0b000111
#define EXCP_EC_PAUTH   0b001001    /* When FEAT_PAuth is implemented */
#define EXCP_EC_SYSCALL 0b010101    /* system call  */
#define EXCP_EC_HVC     0b010110    /* hvc call */
#define	EXCP_EC_SMC     0b010111    /* smc call  */
#define EXCP_EC_MRS     0b011000    /* Trapped MSR, MRS or System instruction execution in AArch64 state, that is not reported using EC 0b000000 , 0b000001 or 0b000111  */ 
#define EXCP_EC_LIA     0b100000    /* Instruction Abort from a lower Exception level */
#define EXCP_EC_SIA     0b100001    /* Instruction Abort taken without a change in Exception level. */ 
#define EXCP_EC_LDA     0b100100    /* Data Abort from a lower Exception level */
#define EXCP_EC_SDA     0b100101    /* Data Abort without a change in Exception level */
#define EXCP_EC_SERROR  0b101111    /* SError interrupt. */

/* those defines refer: Power State Coordination Interface/SMC CALLING CONVENTION   */
#define SMC_64_MASK        (uint64_t)0x40000000
#define SMC_TYPE_MASK      (uint64_t)0x3F000000
#define SMC_SUCCESS             0
#define SMC_NOT_SUPPORTED      -1
#define SMC_INVALID_PARAMETERS -2
#define SMC_DENIED             -3
#define SMC_ALREADY_ON         -4
#define SMC_ON_PENDING         -5
#define SMC_INTERNAL_FAILURE   -6
#define SMC_NOT_PRESENT        -7
#define SMC_DISABLED           -8
#define SMC_INVALID_ADDRESS    -9

#define SMC_TYPE_PSCI_CALL     0x04000000
#define SMC_HYPER_CALL         0x05000000


int hyper_sync_exception(struct arm64_iframe_long *iframe, uint32_t esr);
int32_t hv_psci_call(struct arm64_iframe_long *iframe);

#endif 

