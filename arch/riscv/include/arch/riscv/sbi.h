/*
 * Copyright (c) 2019 Elliot Berman
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#pragma once

#include <lk/compiler.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <arch/riscv.h>

__BEGIN_CDECLS

#if !defined(RISCV_M_MODE) || !(RISCV_M_MODE)

struct sbiret {
    long error;
    long value;
};

enum sbi_return_code {
    SBI_SUCCESS = 0,
    SBI_ERR_FAILURE = -1,
    SBI_ERR_NOT_SUPPORTED = -2,
    SBI_ERR_INVALID_PARAM = -3,
    SBI_ERR_DENIED = -4,
    SBI_ERR_INVALID_ADDRESS = -5,
};

#define SBI_SET_TIMER               0x00, 0
#define SBI_CONSOLE_PUTCHAR         0x01, 0
#define SBI_CONSOLE_GETCHAR         0x02, 0
#define SBI_CLEAR_IPI               0x03, 0
#define SBI_SEND_IPI                0x04, 0
#define SBI_REMOTE_FENCEI           0x05, 0
#define SBI_REMOTE_SFENCE_VMA       0x06, 0
#define SBI_REMOTE_SFENCE_VMA_ASID  0x07, 0
#define SBI_SHUTDOWN                0x08, 0

#define SBI_GET_SBI_SPEC_VERSION    0x10, 0
#define SBI_GET_SBI_IMPL_ID         0x10, 1
#define SBI_GET_SBI_IMPL_VERSION    0x10, 2
#define SBI_PROBE_EXTENSION         0x10, 3
#define SBI_GET_MVENDORID           0x10, 4
#define SBI_GET_MARCHID             0x10, 5
#define SBI_GET_MIMPID              0x10, 6

#define SBI_EXT_TIMER_SIG           0x54494d45
#define SBI_EXT_IPI_SIG             0x00735049
#define SBI_EXT_RFENCE_SIG          0x52464e43
#define SBI_EXT_HSM_SIG             0x0048534d
#define SBI_EXT_SRST_SIG            0x53525354

void sbi_early_init(void);
void sbi_init(void);

void sbi_set_timer(uint64_t stime_value);
void sbi_send_ipis(const unsigned long *hart_mask);
void sbi_clear_ipi(void);
status_t sbi_boot_hart(uint hartid, paddr_t start_addr, ulong arg);

void sbi_rfence_vma(const unsigned long *hart_mask, vaddr_t vma, size_t size);

bool sbi_probe_extension(ulong extension);

struct sbiret sbi_generic_call_2(ulong extension, ulong function);
struct sbiret sbi_generic_call_3(ulong extension, ulong function);

#endif

__END_CDECLS

