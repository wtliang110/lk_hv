/*
 * Copyright (c) 2014 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <arch/mp.h>

#include <assert.h>
#include <lk/trace.h>
#include <lk/err.h>
#include <platform/interrupts.h>
#include <arch/ops.h>

#if WITH_DEV_INTERRUPT_ARM_GIC || WITH_DEV_INTERRUPT_GIC_V3
#include <dev/interrupt/arm_gic.h>
#elif PLATFORM_BCM28XX
/* bcm28xx has a weird custom interrupt controller for MP */
extern void bcm28xx_send_ipi(uint irq, uint cpu_mask);
#else
#error need other implementation of interrupt controller that can ipi
#endif

#define LOCAL_TRACE 0

#define GIC_IPI_BASE (12)

status_t arch_mp_send_ipi(mp_cpu_mask_t target, mp_ipi_t ipi) {
    LTRACEF("target 0x%x, ipi %u\n", target, ipi);

#if WITH_DEV_INTERRUPT_ARM_GIC
    uint gic_ipi_num = ipi + GIC_IPI_BASE;

    /* filter out targets outside of the range of cpus we care about */
    target &= ((1UL << SMP_MAX_CPUS) - 1);
    if (target != 0) {
        LTRACEF("target 0x%x, gic_ipi %u\n", target, gic_ipi_num);
        arm_gic_sgi(gic_ipi_num, ARM_GIC_SGI_FLAG_NS, target);
    }
#elif PLATFORM_BCM28XX
    /* filter out targets outside of the range of cpus we care about */
    target &= ((1UL << SMP_MAX_CPUS) - 1);
    if (target != 0) {
        bcm28xx_send_ipi(ipi, target);
    }
#endif

    return NO_ERROR;
}

enum handler_return arm_ipi_generic_handler(void *arg) {
    LTRACEF("cpu %u, arg %p\n", arch_curr_cpu_num(), arg);

    return INT_NO_RESCHEDULE;
}

enum handler_return arm_ipi_reschedule_handler(void *arg) {
    LTRACEF("cpu %u, arg %p\n", arch_curr_cpu_num(), arg);

    return mp_mbx_reschedule_irq();
}

/* modify by pwang for hyppervisor event handler */
#include <hv_event.h>
#include <hv_vm.h>
void arch_mp_init_percpu(void) {
    register_int_handler(MP_IPI_GENERIC + GIC_IPI_BASE, &arm_ipi_generic_handler, 0);
    register_int_handler(MP_IPI_RESCHEDULE + GIC_IPI_BASE, &arm_ipi_reschedule_handler, 0);
    register_int_handler(MP_IPI_HV_EVENT + GIC_IPI_BASE, &hv_event_handler, 0);
    register_int_handler(MP_IPI_HV_EVENT1 + GIC_IPI_BASE, &hv_event_empty_handler, 0);
    lkhv_mem_init();
    //unmask_interrupt(MP_IPI_GENERIC + GIC_IPI_BASE);
    //unmask_interrupt(MP_IPI_RESCHEDULE + GIC_IPI_BASE);
}

