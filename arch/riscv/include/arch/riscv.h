/*
 * Copyright (c) 2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

#include <lk/compiler.h>
#include <arch/defines.h>
#include <arch/riscv/asm.h>

#define RISCV_USER_OFFSET   (0u)
#define RISCV_SUPER_OFFSET  (1u)
#define RISCV_HYPER_OFFSET  (2u)
#define RISCV_MACH_OFFSET   (3u)

#if RISCV_M_MODE
# define RISCV_XMODE_OFFSET     (RISCV_MACH_OFFSET)
# define RISCV_XRET             mret
#elif RISCV_S_MODE
# define RISCV_XMODE_OFFSET     (RISCV_SUPER_OFFSET)
# define RISCV_XRET             sret
#else
# error Unrecognized RISC-V privilege level selected
#endif

#define RISCV_CSR_XMODE_BITS     (RISCV_XMODE_OFFSET << 8)

// These CSRs are only in user CSR space (still readable by all modes though)
#define RISCV_CSR_CYCLE     (0xc00)
#define RISCV_CSR_TIME      (0xc01)
#define RISCV_CSR_INSRET    (0xc02)
#define RISCV_CSR_CYCLEH    (0xc80)
#define RISCV_CSR_TIMEH     (0xc81)
#define RISCV_CSR_INSRETH   (0xc82)

#define RISCV_CSR_XSTATUS   (0x000 | RISCV_CSR_XMODE_BITS)
#define RISCV_CSR_XIE       (0x004 | RISCV_CSR_XMODE_BITS)
#define RISCV_CSR_XTVEC     (0x005 | RISCV_CSR_XMODE_BITS)
#define RISCV_CSR_XSCRATCH  (0x040 | RISCV_CSR_XMODE_BITS)
#define RISCV_CSR_XEPC      (0x041 | RISCV_CSR_XMODE_BITS)
#define RISCV_CSR_XCAUSE    (0x042 | RISCV_CSR_XMODE_BITS)
#define RISCV_CSR_XTVAL     (0x043 | RISCV_CSR_XMODE_BITS)
#define RISCV_CSR_XIP       (0x044 | RISCV_CSR_XMODE_BITS)

#if RISCV_M_MODE // Machine-mode only CSRs
#define RISCV_CSR_MCYCLE    (0xb00)
#define RISCV_CSR_MVENDORID (0xf11)
#define RISCV_CSR_MARCHID   (0xf12)
#define RISCV_CSR_MIMPID    (0xf13)
#define RISCV_CSR_MHARTID   (0xf14)
#define RISCV_CSR_MISA      (0x301)
#endif // RISCV_M_MODE

#if RISCV_S_MODE // Supervisor-mode only CSRs
#define RISCV_CSR_SATP      (0x180)
#endif

#define RISCV_CSR_XSTATUS_IE    (1ul << (RISCV_XMODE_OFFSET + 0))
#define RISCV_CSR_XSTATUS_PIE   (1ul << (RISCV_XMODE_OFFSET + 4))
#define RISCV_CSR_XSTATUS_SPP   (1ul << 8)
#define RISCV_CSR_XSTATUS_SUM   (1ul << 18)
#define RISCV_CSR_XSTATUS_MXR   (1ul << 19)
#define RISCV_CSR_XSTATUS_FS_SHIFT (13)
#define RISCV_CSR_XSTATUS_FS_MASK (3ul << RISCV_CSR_XSTATUS_FS_SHIFT)

#define RISCV_CSR_XIE_SIE       (1ul << (RISCV_XMODE_OFFSET + 0))
#define RISCV_CSR_XIE_TIE       (1ul << (RISCV_XMODE_OFFSET + 4))
#define RISCV_CSR_XIE_EIE       (1ul << (RISCV_XMODE_OFFSET + 8))

#define RISCV_CSR_XIP_SIP       (1ul << (RISCV_XMODE_OFFSET + 0))
#define RISCV_CSR_XIP_TIP       (1ul << (RISCV_XMODE_OFFSET + 4))
#define RISCV_CSR_XIP_EIP       (1ul << (RISCV_XMODE_OFFSET + 8))

// Interrupts, top bit set in cause register
#define RISCV_INTERRUPT_USWI        0       // software interrupt
#define RISCV_INTERRUPT_SSWI        1
#define RISCV_INTERRUPT_MSWI        3
#define RISCV_INTERRUPT_UTIM        4       // timer interrupt
#define RISCV_INTERRUPT_STIM        5
#define RISCV_INTERRUPT_MTIM        7
#define RISCV_INTERRUPT_UEXT        8       // external interrupt
#define RISCV_INTERRUPT_SEXT        9
#define RISCV_INTERRUPT_MEXT        11

// The 3 interupts above, for the current mode
#define RISCV_INTERRUPT_XSWI        (RISCV_XMODE_OFFSET)
#define RISCV_INTERRUPT_XTIM        (4 + RISCV_XMODE_OFFSET)
#define RISCV_INTERRUPT_XEXT        (8 + RISCV_XMODE_OFFSET)

// Exceptions
#define RISCV_EXCEPTION_IADDR_MISALIGN      0
#define RISCV_EXCEPTION_IACCESS_FAULT       1
#define RISCV_EXCEPTION_ILLEGAL_INS         2
#define RISCV_EXCEPTION_BREAKPOINT          3
#define RISCV_EXCEPTION_LOAD_ADDR_MISALIGN  4
#define RISCV_EXCEPTION_LOAD_ACCESS_FAULT   5
#define RISCV_EXCEPTION_STORE_ADDR_MISALIGN 6
#define RISCV_EXCEPTION_STORE_ACCESS_FAULT  7
#define RISCV_EXCEPTION_ENV_CALL_U_MODE     8
#define RISCV_EXCEPTION_ENV_CALL_S_MODE     9
#define RISCV_EXCEPTION_ENV_CALL_M_MODE     11
#define RISCV_EXCEPTION_INS_PAGE_FAULT      12
#define RISCV_EXCEPTION_LOAD_PAGE_FAULT     13
#define RISCV_EXCEPTION_STORE_PAGE_FAULT    15

#ifndef ASSEMBLY
#define __ASM_STR(x)    #x

#define riscv_csr_clear(csr, bits) \
({ \
    ulong __val = bits; \
    __asm__ volatile( \
        "csrc   " __ASM_STR(csr) ", %0" \
        :: "rK" (__val) \
        : "memory"); \
})

#define riscv_csr_read_clear(csr, bits) \
({ \
    ulong __val = bits; \
    ulong __val_out; \
    __asm__ volatile( \
        "csrrc   %0, " __ASM_STR(csr) ", %1" \
        : "=r"(__val_out) \
        : "rK" (__val) \
        : "memory"); \
    __val_out; \
})

#define riscv_csr_set(csr, bits) \
({ \
    ulong __val = bits; \
    __asm__ volatile( \
        "csrs   " __ASM_STR(csr) ", %0" \
        :: "rK" (__val) \
        : "memory"); \
})

#define riscv_csr_read(csr) \
({ \
    ulong __val; \
    __asm__ volatile( \
        "csrr   %0, " __ASM_STR(csr) \
        : "=r" (__val) \
        :: "memory"); \
    __val; \
})

#define riscv_csr_write(csr, val) \
({ \
    ulong __val = (ulong)val; \
    __asm__ volatile( \
        "csrw   " __ASM_STR(csr) ", %0" \
        :: "rK" (__val) \
        : "memory"); \
    __val; \
})

struct riscv_percpu {
    // must be first field in the struct
    struct thread *curr_thread;
    unsigned int cpu_num;
    unsigned int hart_id;
} __ALIGNED(CACHE_LINE);

// percpu pointer is held in the tp register while in the kernel
static inline struct riscv_percpu *riscv_get_percpu(void) {
    struct riscv_percpu *cpu;
    __asm__ volatile("mv %0, tp" : "=&r"(cpu));
    return cpu;
}

static inline void riscv_set_percpu(struct riscv_percpu *cpu) {
    __asm__ volatile("mv tp, %0" :: "r"(cpu));
}

// current thread is always at the start of the percpu struct
static inline struct thread *riscv_get_current_thread(void) {
    struct thread *t;
#if __riscv_xlen == 32
    __asm__ volatile("lw %0, 0(tp)" : "=&r"(t));
#else
    __asm__ volatile("ld %0, 0(tp)" : "=&r"(t));
#endif
    return t;
}

static inline void riscv_set_current_thread(struct thread *t) {
#if __riscv_xlen == 32
    __asm__ volatile("sw %0, 0(tp)" :: "r"(t));
#else
    __asm__ volatile("sd %0, 0(tp)" :: "r"(t));
#endif
}

static inline uint riscv_current_hart(void) {
#if RISCV_M_MODE
    return riscv_csr_read(RISCV_CSR_MHARTID);
#else
    return riscv_get_percpu()->hart_id;
#endif
}

void riscv_set_secondary_count(int count);

void riscv_exception_entry(void);
enum handler_return riscv_timer_exception(void);

// If using S mode, time seems to be implemented in clint.h
// TODO: clean up by moving into its own header
#if RISCV_S_MODE
# if __riscv_xlen == 32
static inline uint64_t riscv_get_time(void) {
    uint32_t hi, lo;

    do {
        hi = riscv_csr_read(RISCV_CSR_TIMEH);
        lo = riscv_csr_read(RISCV_CSR_TIME);
    } while (hi != riscv_csr_read(RISCV_CSR_TIMEH));

    return (((uint64_t)hi << 32) | lo);
}
# else
static inline uint64_t riscv_get_time(void) {
    return riscv_csr_read(RISCV_CSR_TIME);
}
# endif
#endif

#endif /* ASSEMBLY */
