/*
 * Copyright (c) 2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <arch/riscv/sbi.h>

#include <stdbool.h>
#include <stdio.h>
#include <lk/debug.h>
#include <lk/trace.h>
#include <lk/err.h>
#include <arch/riscv.h>

#include "riscv_priv.h"

#if RISCV_S_MODE

// bitmap of locally detected SBI extensions
enum sbi_extension {
    SBI_EXTENSION_TIMER = 1,
    SBI_EXTENSION_IPI,
    SBI_EXTENSION_RFENCE,
    SBI_EXTENSION_HSM,
    SBI_EXTENSION_SRST,
};

static uint sbi_ext;

// make a SBI call according to the SBI spec at https://github.com/riscv/riscv-sbi-doc
// Note: it seems ambigious whether or not a2-a7 are trashed in the call, but the
// OpenSBI and linux implementations seem to assume that all of the regs are restored
// aside from a0 and a1 which are used for return values.
#define _sbi_call(extension, function, arg0, arg1, arg2, arg3, arg4, arg5, ...) ({  \
    register unsigned long a0 asm("a0") = (unsigned long)arg0;      \
    register unsigned long a1 asm("a1") = (unsigned long)arg1;      \
    register unsigned long a2 asm("a2") = (unsigned long)arg2;      \
    register unsigned long a3 asm("a3") = (unsigned long)arg3;      \
    register unsigned long a4 asm("a4") = (unsigned long)arg4;      \
    register unsigned long a5 asm("a5") = (unsigned long)arg5;      \
    register unsigned long a6 asm("a6") = (unsigned long)function;      \
    register unsigned long a7 asm("a7") = (unsigned long)extension;     \
    asm volatile ("ecall"                       \
        : "+r" (a0), "+r" (a1)                  \
        : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r"(a6), "r"(a7) \
        : "memory");                        \
    (struct sbiret){ .error = a0, .value = a1 };       \
    })
#define sbi_call(...) \
    _sbi_call(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0)

static inline bool sbi_ext_present(enum sbi_extension e) {
    return sbi_ext & (1 << e);
}

struct sbiret sbi_generic_call_2(ulong extension, ulong function) {
    return sbi_call(extension, function);
}

bool sbi_probe_extension(ulong extension) {
    return sbi_call(SBI_PROBE_EXTENSION, extension).value != 0;
}

void sbi_set_timer(uint64_t stime_value) {
    // use the new IPI extension
    if (likely(sbi_ext_present(SBI_EXTENSION_TIMER))) {
        sbi_call(SBI_EXT_TIMER_SIG, 0, stime_value);
    } else {
        sbi_call(SBI_SET_TIMER, stime_value);
    }
}

void sbi_send_ipis(const unsigned long *hart_mask) {
    // use the new IPI extension
    if (likely(sbi_ext_present(SBI_EXTENSION_IPI))) {
        sbi_call(SBI_EXT_IPI_SIG, 0, *hart_mask, -1);
    } else {
        // legacy ipi call
        sbi_call(SBI_SEND_IPI, hart_mask);
    }
}

void sbi_clear_ipi(void) {
    // deprecated, clear sip.SSIP directly
    riscv_csr_clear(RISCV_CSR_XIP, RISCV_CSR_XIP_SIP);
    //sbi_call(SBI_CLEAR_IPI);
}

status_t sbi_boot_hart(uint hartid, paddr_t start_addr, ulong arg) {
    if (!sbi_ext_present(SBI_EXTENSION_HSM))
        return ERR_NOT_IMPLEMENTED;

    // try to use the HSM implementation to boot a cpu
    struct sbiret ret = sbi_call(SBI_EXT_HSM_SIG, 0, hartid, start_addr, arg);
    if (ret.error < 0) {
        return ERR_INVALID_ARGS;
    }

    return NO_ERROR;
}

void sbi_rfence_vma(const unsigned long *hart_mask, vaddr_t vma, size_t size) {
    // use the new IPI extension
    if (likely(sbi_ext_present(SBI_EXTENSION_RFENCE))) {
        sbi_call(SBI_EXT_RFENCE_SIG, 1, *hart_mask, 0, vma, size);
    } else {
        PANIC_UNIMPLEMENTED;
    }
}

void sbi_early_init(void) {
    // read the presence of some features
    sbi_ext |= sbi_probe_extension(SBI_EXT_TIMER_SIG) ? (1<<SBI_EXTENSION_TIMER) : 0;
    sbi_ext |= sbi_probe_extension(SBI_EXT_IPI_SIG) ? (1<<SBI_EXTENSION_IPI) : 0;
    sbi_ext |= sbi_probe_extension(SBI_EXT_RFENCE_SIG) ? (1<<SBI_EXTENSION_RFENCE) : 0;
    sbi_ext |= sbi_probe_extension(SBI_EXT_HSM_SIG) ? (1<<SBI_EXTENSION_HSM) : 0;
    sbi_ext |= sbi_probe_extension(SBI_EXT_SRST_SIG) ? (1<<SBI_EXTENSION_SRST) : 0;
}

void sbi_init(void) {
    dprintf(INFO, "RISCV: SBI spec version %ld impl id %ld version %ld\n",
            sbi_generic_call_2(SBI_GET_SBI_SPEC_VERSION).value,
            sbi_generic_call_2(SBI_GET_SBI_IMPL_ID).value,
            sbi_generic_call_2(SBI_GET_SBI_IMPL_VERSION).value);

    // print the extensions detected
    dprintf(INFO, "RISCV: SBI extension TIMER %d\n", sbi_ext_present(SBI_EXTENSION_TIMER));
    dprintf(INFO, "RISCV: SBI extension IPI %d\n", sbi_ext_present(SBI_EXTENSION_IPI));
    dprintf(INFO, "RISCV: SBI extension RFENCE %d\n", sbi_ext_present(SBI_EXTENSION_RFENCE));
    dprintf(INFO, "RISCV: SBI extension HSM %d\n", sbi_ext_present(SBI_EXTENSION_HSM));
    dprintf(INFO, "RISCV: SBI extension SRST %d\n", sbi_ext_present(SBI_EXTENSION_SRST));

    // print a line via the console
    const char test[] = "SBI console test\n\r";
    for (const char *c = test; *c != 0; c++) {
        sbi_call(SBI_CONSOLE_PUTCHAR, *c);
    }
}

#endif

