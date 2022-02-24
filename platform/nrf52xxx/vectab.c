/*
 * Copyright (c) 2012 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <lk/debug.h>
#include <lk/compiler.h>
#include <arch/arm/cm.h>

/* un-overridden irq handler */
void nrf52_dummy_irq(void) {
    arm_cm_irq_entry();
    panic("unhandled irq\n");
}

/* a list of default handlers that are simply aliases to the dummy handler */
#define DEFAULT_HANDLER(x) \
void nrf52_##x(void) __WEAK_ALIAS("nrf52_dummy_irq");

DEFAULT_HANDLER(POWER_CLOCK_IRQ);
DEFAULT_HANDLER(RADIO_IRQ);
DEFAULT_HANDLER(UARTE0_UART0_IRQ);
DEFAULT_HANDLER(SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQ);
DEFAULT_HANDLER(SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQ);
DEFAULT_HANDLER(NFCT_IRQ);
DEFAULT_HANDLER(GPIOTE_IRQ);
DEFAULT_HANDLER(SAADC_IRQ);
DEFAULT_HANDLER(TIMER0_IRQ);
DEFAULT_HANDLER(TIMER1_IRQ);
DEFAULT_HANDLER(TIMER2_IRQ);
DEFAULT_HANDLER(RTC0_IRQ);
DEFAULT_HANDLER(TEMP_IRQ);
DEFAULT_HANDLER(RNG_IRQ);
DEFAULT_HANDLER(ECB_IRQ);
DEFAULT_HANDLER(CCM_AAR_IRQ);
DEFAULT_HANDLER(WDT_IRQ);
DEFAULT_HANDLER(RTC1_IRQ);
DEFAULT_HANDLER(QDEC_IRQ);
DEFAULT_HANDLER(COMP_LPCOMP_IRQ);
DEFAULT_HANDLER(SWI0_EGU0_IRQ);
DEFAULT_HANDLER(SWI1_EGU1_IRQ);
DEFAULT_HANDLER(SWI2_EGU2_IRQ);
DEFAULT_HANDLER(SWI3_EGU3_IRQ);
DEFAULT_HANDLER(SWI4_EGU4_IRQ);
DEFAULT_HANDLER(SWI5_EGU5_IRQ);
DEFAULT_HANDLER(TIMER3_IRQ);
DEFAULT_HANDLER(TIMER4_IRQ);
DEFAULT_HANDLER(PWM0_IRQ);
DEFAULT_HANDLER(PDM_IRQ);
DEFAULT_HANDLER(MWU_IRQ);
DEFAULT_HANDLER(PWM1_IRQ);
DEFAULT_HANDLER(PWM2_IRQ);
DEFAULT_HANDLER(SPIM2_SPIS2_SPI2_IRQ);
DEFAULT_HANDLER(RTC2_IRQ);
DEFAULT_HANDLER(I2S_IRQ);
DEFAULT_HANDLER(FPU_IRQ);
DEFAULT_HANDLER(USBD_IRQ);
DEFAULT_HANDLER(UARTE1_IRQ);
DEFAULT_HANDLER(QSPI_IRQ);
DEFAULT_HANDLER(CRYPTOCELL_IRQ);
DEFAULT_HANDLER(PWM3_IRQ);
DEFAULT_HANDLER(SPIM3_IRQ);


#define VECTAB_ENTRY(x) [x##n] = nrf52_##x

/* appended to the end of the main vector table */
const void *const __SECTION(".text.boot.vectab2") vectab2[] = {

    VECTAB_ENTRY(POWER_CLOCK_IRQ),
    VECTAB_ENTRY(RADIO_IRQ),
    VECTAB_ENTRY(UARTE0_UART0_IRQ),
    VECTAB_ENTRY(SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQ),
    VECTAB_ENTRY(SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQ),
    VECTAB_ENTRY(NFCT_IRQ),
    VECTAB_ENTRY(GPIOTE_IRQ),
    VECTAB_ENTRY(SAADC_IRQ),
    VECTAB_ENTRY(TIMER0_IRQ),
    VECTAB_ENTRY(TIMER1_IRQ),
    VECTAB_ENTRY(TIMER2_IRQ),
    VECTAB_ENTRY(RTC0_IRQ),
    VECTAB_ENTRY(TEMP_IRQ),
    VECTAB_ENTRY(RNG_IRQ),
    VECTAB_ENTRY(ECB_IRQ),
    VECTAB_ENTRY(CCM_AAR_IRQ),
    VECTAB_ENTRY(WDT_IRQ),
    VECTAB_ENTRY(RTC1_IRQ),
    VECTAB_ENTRY(QDEC_IRQ),
    VECTAB_ENTRY(COMP_LPCOMP_IRQ),
    VECTAB_ENTRY(SWI0_EGU0_IRQ),
    VECTAB_ENTRY(SWI1_EGU1_IRQ),
    VECTAB_ENTRY(SWI2_EGU2_IRQ),
    VECTAB_ENTRY(SWI3_EGU3_IRQ),
    VECTAB_ENTRY(SWI4_EGU4_IRQ),
    VECTAB_ENTRY(SWI5_EGU5_IRQ),
    VECTAB_ENTRY(TIMER3_IRQ),
    VECTAB_ENTRY(TIMER4_IRQ),
    VECTAB_ENTRY(PWM0_IRQ),
    VECTAB_ENTRY(PDM_IRQ),
    NULL,
    NULL,
    VECTAB_ENTRY(MWU_IRQ),
    VECTAB_ENTRY(PWM1_IRQ),
    VECTAB_ENTRY(PWM2_IRQ),
    VECTAB_ENTRY(SPIM2_SPIS2_SPI2_IRQ),
    VECTAB_ENTRY(RTC2_IRQ),
    VECTAB_ENTRY(I2S_IRQ),
    VECTAB_ENTRY(FPU_IRQ),
    VECTAB_ENTRY(USBD_IRQ),
    VECTAB_ENTRY(UARTE1_IRQ),
    VECTAB_ENTRY(QSPI_IRQ),
    VECTAB_ENTRY(CRYPTOCELL_IRQ),
    NULL,
    NULL,
    VECTAB_ENTRY(PWM3_IRQ),
    NULL,
    VECTAB_ENTRY(SPIM3_IRQ),
};

