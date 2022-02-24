/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#ifndef __DEV_INTERRUPT_ARM_GIC_H
#define __DEV_INTERRUPT_ARM_GIC_H

#include <sys/types.h>
#include <platform/interrupts.h>

void arm_gic_init(void);

#define GIC_BASE_SGI 0
#define GIC_BASE_PPI 16
#define GIC_BASE_SPI 32

/* interrupt handler callback parameter which requires vector as args  */
#define INT_IRQ  0xabcddcba

enum interrupt_trigger_mode {
    IRQ_TRIGGER_MODE_EDGE = 0,
    IRQ_TRIGGER_MODE_LEVEL = 1,
};

enum interrupt_polarity {
    IRQ_POLARITY_ACTIVE_HIGH = 0,
    IRQ_POLARITY_ACTIVE_LOW = 1,
};

enum {
    /* Ignore cpu_mask and forward interrupt to all CPUs other than the current cpu */
    ARM_GIC_SGI_FLAG_TARGET_FILTER_NOT_SENDER = 0x1,
    /* Ignore cpu_mask and forward interrupt to current CPU only */
    ARM_GIC_SGI_FLAG_TARGET_FILTER_SENDER = 0x2,
    ARM_GIC_SGI_FLAG_TARGET_FILTER_MASK = 0x3,

    /* Only forward the interrupt to CPUs that has the interrupt configured as group 1 (non-secure) */
    ARM_GIC_SGI_FLAG_NS = 0x4,
};

/* cpu register defition  */
#define GICREG(reg) (*REG32(reg))
#define GICREG_64(reg) (*REG64(reg))
#define GICREG_BYTE(reg) (*REG8(reg))
#define GICR_FRAME                      0x10000

/*  gicd in none secure mode */
#define GICD_CTRL_RWP          (uint32_t) (1 << 31)
#define GICD_CTRL_ARE_NS       (uint32_t) (1 << 4)
#define GICD_CTRL_EnableGrp1NS (uint32_t) (1 << 1)
#define GICD_CTRL_EnableGrp1   (uint32_t) (1 << 0)
#define GICD_CTRL_DS           (uint32_t) (1 << 6)

#define GIC_PRI_LOWEST     0xb0
#define GIC_PRI_IRQ        0xa0
#define GIC_PRI_IPI        0x90           /* IPIs must preempt normal interrupts */
#define GIC_PRI_HV         0x85           /* Higher priorities belong to Secure-World */
#define GIC_PRI_HIGHEST    0x80           /* Higher priorities belong to Secure-World */

status_t arm_gic_sgi(u_int irq, u_int flags, u_int cpu_mask);
uint32_t get_max_spi(void);
uint32_t read_typer(void);
uint32_t get_max_spi(void);
void gic_set_enable(uint vector, bool enable); 
uint32_t read_gicd_iidr(void);
void gic_trigger_interrupt(int vector); 
void gic_dump_status(uint32_t vector, int cpu_id);
void gic_clear_pending(int vector);
void gic_set_cfg(uint32_t vector, int is_edge, int cpu_id);
uint32_t read_gicr_iidr(void);
void gic_set_affinity(uint32_t vector, int cpu_id, int is_cluster);
int gic_set_irq(uint32_t vector, int priority, 
		int is_edge, int enable,
		int_handler handler, void *arg);
void gic_set_icactiver(int vector);
void gic_clear_pending(int vector);
int gic_read_ispendr(unsigned int vector);
#endif

