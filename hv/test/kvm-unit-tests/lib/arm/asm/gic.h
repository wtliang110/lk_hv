/*
 * Copyright (C) 2016, Red Hat Inc, Andrew Jones <drjones@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.
 */
#ifndef _ASMARM_GIC_H_
#define _ASMARM_GIC_H_

#define GIC_NR_PRIVATE_IRQS		32
#define GIC_FIRST_SPI			GIC_NR_PRIVATE_IRQS

/* Distributor registers */
#define GICD_CTLR			0x0000
#define GICD_TYPER			0x0004
#define GICD_IIDR			0x0008
#define GICD_TYPER2			0x000C
#define GICD_IGROUPR			0x0080
#define GICD_ISENABLER			0x0100
#define GICD_ICENABLER			0x0180
#define GICD_ISPENDR			0x0200
#define GICD_ICPENDR			0x0280
#define GICD_ISACTIVER			0x0300
#define GICD_ICACTIVER			0x0380
#define GICD_IPRIORITYR			0x0400
#define GICD_ITARGETSR			0x0800
#define GICD_SGIR			0x0f00
#define GICD_ICPIDR2			0x0fe8
#define GICD_ICFGR                      0x0c00
#define GICD_IROUTER                    0x6100

#define GICD_TYPER_IRQS(typer)		((((typer) & 0x1f) + 1) * 32)
#define GICD_INT_EN_SET_SGI		0x0000ffff
#define GICD_INT_DEF_PRI_X4		0xa0a0a0a0

/* CPU interface registers */
#define GICC_CTLR			0x0000
#define GICC_PMR			0x0004
#define GICC_IAR			0x000c
#define GICC_EOIR			0x0010

#define GICC_INT_PRI_THRESHOLD		0xf0
#define GICC_INT_SPURIOUS		0x3ff

#include <asm/gic-v2.h>
#include <asm/gic-v3.h>
#include <asm/gic-v3-its.h>

#define PPI(irq)			((irq) + 16)
#define SPI(irq)			((irq) + GIC_FIRST_SPI)

#ifndef __ASSEMBLY__
#include <asm/cpumask.h>

enum gic_irq_state {
	GIC_IRQ_STATE_INACTIVE,
	GIC_IRQ_STATE_PENDING,
	GIC_IRQ_STATE_ACTIVE,
	GIC_IRQ_STATE_ACTIVE_PENDING,
};

/*
 * gic_init will try to find all known gics, and then
 * initialize the gic data for the one found.
 * returns
 *  0   : no gic was found
 *  > 0 : the gic version of the gic found
 */
extern int gic_init(void);

/*
 * gic_enable_defaults enables the gic with basic but useful
 * settings. gic_enable_defaults will call gic_init if it has
 * not yet been invoked.
 */
extern void gic_enable_defaults(void);

/*
 * After enabling the gic with gic_enable_defaults the functions
 * below will work with any supported gic version.
 */
extern int gic_version(void);
extern u32 gic_read_iar(void);
extern u32 gic_iar_irqnr(u32 iar);
extern void gic_write_eoir(u32 irqstat);
extern void gic_ipi_send_single(int irq, int cpu);
extern void gic_ipi_send_mask(int irq, const cpumask_t *dest);
extern enum gic_irq_state gic_irq_state(int irq);

void gic_irq_set_clr_enable(int irq, bool enable);
#define gic_enable_irq(irq) gic_irq_set_clr_enable(irq, true)
#define gic_disable_irq(irq) gic_irq_set_clr_enable(irq, false)

#endif /* !__ASSEMBLY__ */
#endif /* _ASMARM_GIC_H_ */
