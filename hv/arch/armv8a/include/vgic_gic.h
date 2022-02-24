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

#ifndef __VGIC_GIC_H__
#define __VGIC_GIC_H__
#include <stdint.h>

#define LR_INDEX_INVALID           (uint8_t)(0xff)
#define ICH_HCR_EN                 (uint64_t)(1 << 0)   /* virtual cpu interface is enable  */
#define ICH_HCR_UIE    		   (uint64_t)(1 << 1)   /* Underflow Interrupt Enable */
#define ICH_HCR_LRENPIE            (uint64_t)(1 << 2)   /* List Register Entry Not Present Interrupt Enable  */
#define ICH_HCR_NPIE               (uint64_t)(1 << 3)   /* No Pending Interrupt Enable  */
#define ICH_HCR_VGrp0EIE           (uint64_t)(1 << 4)   /* No Pending Interrupt Enable  */
#define ICH_HCR_VGrp0DIE           (uint64_t)(1 << 5)   /* VM Group 0 Disabled Interrupt Enable  */
#define ICH_HCR_VGrp1EIE           (uint64_t)(1 << 6)   /* VM Group 1 Enabled Interrupt Enable  */
#define ICH_HCR_VGrp1DIE           (uint64_t)(1 << 7)   /* VM Group 1 Disabled Interrupt Enable  */
#define ICH_HCR_TC                 (uint64_t)(1 << 10)  /* Trap all EL1 accesses to System registers that are common to Group 0 and Group 1 to EL2  */
#define ICH_HCR_TALL0              (uint64_t)(1 << 4)   /* Trap all EL1 accesses to ICC_* and ICV_* System registers for Group 0 interrupts to EL2  */

#define ICH_VMCR_VENG0             (uint64_t)(1 << 0)   /* Virtual Group 0 interrupt enable  */
#define ICH_VMCR_VENG1             (uint64_t)(1 << 1)   /* Virtual Group 1 interrupt enable */
#define ICH_VMCR_VAckCtl           (uint64_t)(1 << 2)   /* Virtual AckCtl  */

/* interrupt reason  */
#define MAINT_UIE                  ICH_HCR_UIE
#define MAINT_LRENPIE              ICH_HCR_LRENPIE
#define MAINT_NPIE                 ICH_HCR_NPIE
#define MAINT_VGrp1DIE             ICH_HCR_VGrp1DIE 
#define MAINT_VGrp1EIE             ICH_HCR_VGrp1EIE
#define MAINT_VGrp0DIE             ICH_HCR_VGrp0DIE
#define MAINT_VGrp0EIE             ICH_HCR_VGrp0EIE

/*
 * Decode LR register content.
 * The LR register format is different for GIC HW version
 */
struct gic_lr {
   /* Virtual IRQ */
   uint32_t virq;
   uint8_t  priority;
   uint8_t  status;
   uint16_t pirq;
};

#define LR_ST_INVALID 0x0
#define LR_ST_PENDING 0x1
#define LR_ST_ACTIVE  0x2
#define LR_ST_ACT_PEND 0x3
#define LR_NR  16             /* gic v3 is 16 */
#define AP_NR  4              /* gic v3 is 4  */

#define ICH_ELRSR_EL2           S3_4_C12_C11_5
#define ICH_VMCR_EL2   		S3_4_C12_C11_7
#define ICH_HCR_EL2             S3_4_C12_C11_0


struct vgic_irq;
uint8_t vgic_get_lr(void);
void vgic_update_lr(struct vgic_irq *virq);
void vgic_write_lr(struct vgic_irq *virq);
void vgic_read_lr(uint8_t lr, struct gic_lr *val);
void vgic_maintenace_on(int reason);
void vgic_maintenace_off(int reason);
void vgic_read_lr(uint8_t lr, struct gic_lr *val);
uint64_t vgic_read_lr_reg(uint8_t lr);
void vgic_write_lr_reg(uint8_t lr, uint64_t val);
uint64_t vgic_read_aprn_reg(uint8_t nr, uint8_t is_group0);
void vgic_write_aprn_reg(uint8_t nr, uint8_t is_group0, uint64_t val); 
uint8_t vgic_get_pribits(void);
int  vgic_gic_init(void);
uint8_t vgic_get_lr_nr(void);	
#endif 
