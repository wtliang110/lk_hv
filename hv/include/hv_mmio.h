/* mmio header file
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

#ifndef __HV_MMIO_H__
#define __HV_MMIO_H__

#include <rbtree.h>
#include <arch/spinlock.h>

struct lk_vm;
struct lk_vcpu;

/*
 *
 * @vcpu: the context which mmio happens
 * @pga: the guest physical address to read or write 
 * @len: the memory len 
 * @date: to write to for mmio_read or read from for mmio_write
 *
 * */
typedef int (*mmio_read_handler)(struct lk_vcpu *vcpu, paddr_t gpa, int len, uint64_t *data, void *arg);
typedef int (*mmio_write_handler)(struct lk_vcpu *vcpu, paddr_t gpa, int len, uint64_t data, void *arg);

struct mmio_space {
	struct rb_root mmio_root;
	spin_lock_t lock;
};

struct mmio_ops {
	mmio_read_handler read;
	mmio_write_handler write;
};

int hv_mmio_handler(paddr_t addr, int len, int is_write, uint64_t date, int is_signed);
int hv_mmio_register(struct lk_vm *vm, paddr_t addr, uint64_t len, struct mmio_ops *mops, void *args);
void hv_vm_mmio_init(struct lk_vm *vm);

#endif 
