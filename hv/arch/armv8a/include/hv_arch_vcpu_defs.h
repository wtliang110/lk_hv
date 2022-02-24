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

#ifndef __HV_ARCH_VCPU_DEFS__
#define __HV_ARCH_VCPU_DEFS__

/*
 * The bits we set in HCR:
 * TLOR:	Trap LORegion register accesses
 * RW:		64bit by default, can be overridden for 32bit VMs
 * TAC:		Trap ACTLR
 * TSC:		Trap SMC
 * TSW:		Trap cache operations by set/way
 * TWE:		Trap WFE
 * TWI:		Trap WFI
 * TIDCP:	Trap L2CTLR/L2ECTLR
 * BSU_IS:	Upgrade barriers to the inner shareable domain
 * FB:		Force broadcast of all maintenance operations
 * AMO:		Override CPSR.A and enable signaling with VA
 * IMO:		Override CPSR.I and enable signaling with VI
 * FMO:		Override CPSR.F and enable signaling with VF
 * SWIO:	Turn set/way invalidates into set/way clean+invalidate
 * PTW:		Take a stage2 fault if a stage1 walk steps in device memory
 */

#ifdef __ASSEMBLY__
#define _AC(X,Y)	X
#define _AT(T,X)	X
#else
#define __AC(X,Y)	(X##Y)
#define _AC(X,Y)	__AC(X,Y)
#define _AT(T,X)	((T)(X))
#endif

#define HCR_RW          (_AC(1,UL)<<31) /* Register Width, ARM64 only */
#define HCR_TGE         (_AC(1,UL)<<27) /* Trap General Exceptions */
#define HCR_TVM         (_AC(1,UL)<<26) /* Trap Virtual Memory Controls */
#define HCR_TTLB        (_AC(1,UL)<<25) /* Trap TLB Maintenance Operations */
#define HCR_TPU         (_AC(1,UL)<<24) /* Trap Cache Maintenance Operations to PoU */
#define HCR_TPC         (_AC(1,UL)<<23) /* Trap Cache Maintenance Operations to PoC */
#define HCR_TSW         (_AC(1,UL)<<22) /* Trap Set/Way Cache Maintenance Operations */
#define HCR_TAC         (_AC(1,UL)<<21) /* Trap ACTLR Accesses */
#define HCR_TIDCP       (_AC(1,UL)<<20) /* Trap lockdown */
#define HCR_TSC         (_AC(1,UL)<<19) /* Trap SMC instruction */
#define HCR_TID3        (_AC(1,UL)<<18) /* Trap ID Register Group 3 */
#define HCR_TID2        (_AC(1,UL)<<17) /* Trap ID Register Group 2 */
#define HCR_TID1        (_AC(1,UL)<<16) /* Trap ID Register Group 1 */
#define HCR_TID0        (_AC(1,UL)<<15) /* Trap ID Register Group 0 */
#define HCR_TWE         (_AC(1,UL)<<14) /* Trap WFE instruction */
#define HCR_TWI         (_AC(1,UL)<<13) /* Trap WFI instruction */
#define HCR_DC          (_AC(1,UL)<<12) /* Default cacheable */
#define HCR_BSU_MASK    (_AC(3,UL)<<10) /* Barrier Shareability Upgrade */
#define HCR_BSU_NONE    (_AC(0,UL)<<10)
#define HCR_BSU_IS      (_AC(1,UL)<<10)
#define HCR_BSU_OS      (_AC(2,UL)<<10)
#define HCR_BSU_FULL    (_AC(3,UL)<<10)
#define HCR_FB          (_AC(1,UL)<<9) /* Force Broadcast of Cache/BP/TLB operations */
#define HCR_VA          (_AC(1,UL)<<8) /* Virtual Asynchronous Abort */
#define HCR_VI          (_AC(1,UL)<<7) /* Virtual IRQ */
#define HCR_VF          (_AC(1,UL)<<6) /* Virtual FIQ */
#define HCR_AMO         (_AC(1,UL)<<5) /* Override CPSR.A */
#define HCR_IMO         (_AC(1,UL)<<4) /* Override CPSR.I */
#define HCR_FMO         (_AC(1,UL)<<3) /* Override CPSR.F */
#define HCR_PTW         (_AC(1,UL)<<2) /* Protected Walk */
#define HCR_SWIO        (_AC(1,UL)<<1) /* Set/Way Invalidation Override */
#define HCR_VM          (_AC(1,UL)<<0) /* Virtual MMU Enable */

#define HCR_GUEST_DEFAULT (HCR_TSC | HCR_VM | HCR_BSU_IS | HCR_FB |\
			 HCR_AMO | HCR_SWIO | HCR_RW | \
			 HCR_FMO | HCR_IMO | HCR_PTW)
#define HSTR_DEFAULT_VALUE 0
#define CPTR_DEFAULT_VALUE 0
#define CPACR_DEFAULT_VALUE 0

/*  23: Set Privileged Access Never, on taking an exception to EL1
 *  22: Exception Entry is Context Synchronizing.
 *  18: Traps EL0 execution of WFE instructions to EL1, or to EL2
 *  16: Traps EL0 execution of WFI instructions to EL1, or to EL2
 *  11: Exception Exit is Context Synchronizing
 *  5:  System instruction memory barrier enable
 *  4:  SP Alignment check enable for EL0
 *  3:  SP Alignment check enable
 *
 *  */
#define GUEST_SCTLR_EL1 0xc50838


/* 8:SError interrupt mask
 * 7:IRQ interrupt mask
 * 6:FIQ interrupt mask  
 * M:el1h*/
#define SPSR_EL2_DEFAULT 0xc5 
#endif 
