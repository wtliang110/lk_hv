/*
 * Copyright (c) 2014 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

#include <stdbool.h>
#include <sys/types.h>
#include <lk/compiler.h>
#include <arch/arch_thread.h>

__BEGIN_CDECLS

#define DSB __asm__ volatile("dsb sy" ::: "memory")
#define ISB __asm__ volatile("isb" ::: "memory")

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define ARM64_READ_SYSREG(reg) \
({ \
    uint64_t _val; \
    __asm__ volatile("mrs %0," TOSTRING(reg) : "=r" (_val)); \
    _val; \
})

#define ARM64_WRITE_SYSREG(reg, val) \
({ \
    __asm__ volatile("msr " TOSTRING(reg) ", %0" :: "r" (val)); \
    ISB; \
})

void arm64_context_switch(vaddr_t *old_sp, vaddr_t new_sp);

/* exception handling */
struct arm64_iframe_long {
    uint64_t r[30];
    uint64_t lr;
    uint64_t usp;
    uint64_t elr;
    uint64_t spsr;
};

struct arm64_iframe_short {
    uint64_t r[18];
    uint64_t lr;
    uint64_t usp;
    uint64_t elr;
    uint64_t spsr;
};

struct arm64_stackframe {
    uint64_t fp;
    uint64_t pc;
};

struct thread;
extern void arm64_exception_base(void);
void arm64_el3_to_el1(void);
void arm64_fpu_exception(struct arm64_iframe_long *iframe);
void arm64_fpu_save_state(struct thread *thread);
void arm64_fpu_load_state(struct thread *t);
void arm64_fpu_set_capcr(struct thread *t, uint32_t cpacr);
uint32_t arm64_fpu_get_capcr(struct thread *t);


static inline void arm64_fpu_pre_context_switch(struct thread *thread) {
    uint32_t cpacr = ARM64_READ_SYSREG(cpacr_el1);
    if ((cpacr >> 20) & 3) {
        arm64_fpu_save_state(thread);
        //cpacr &= ~(3 << 20);
        //ARM64_WRITE_SYSREG(cpacr_el1, cpacr);
	arm64_fpu_set_capcr(thread, cpacr);
    }
}

static inline void arm64_fpu_post_context_switch(struct thread *thread) {
    uint32_t cpacr = arm64_fpu_get_capcr(thread);
    ARM64_WRITE_SYSREG(cpacr_el1, cpacr);
    if ((cpacr >> 20) & 3) {
        arm64_fpu_load_state(thread);
    }
}

/* overridable syscall handler */
void arm64_syscall(struct arm64_iframe_long *iframe, bool is_64bit);

__END_CDECLS

