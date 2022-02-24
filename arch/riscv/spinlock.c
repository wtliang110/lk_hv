/*
 * Copyright (c) 2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <arch/spinlock.h>

// super simple spin lock implementation

int riscv_spin_trylock(spin_lock_t *lock) {
    unsigned long val = arch_curr_cpu_num() + 1UL;
    unsigned long old;

    __asm__ __volatile__(
        "amoswap.w  %0, %2, %1\n"
        "fence      r, rw\n"
        : "=r"(old), "+A"(*lock)
        : "r" (val)
        : "memory"
    );

    return !old;
}

void riscv_spin_lock(spin_lock_t *lock) {
    unsigned long val = arch_curr_cpu_num() + 1UL;

    for (;;) {
        if (*lock) {
            continue;
        }

        unsigned long old;
        __asm__ __volatile__(
            "amoswap.w  %0, %2, %1\n"
            "fence      r, rw\n"
            : "=r"(old), "+A"(*lock)
            : "r" (val)
            : "memory"
        );

        if (!old) {
            break;
        }
    }
}

void riscv_spin_unlock(spin_lock_t *lock) {
    __asm__ volatile("fence rw,w" ::: "memory");
    *lock = 0;
}

