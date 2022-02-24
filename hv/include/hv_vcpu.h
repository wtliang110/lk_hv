/* @author pwang
 * @mail wtliang110@aliyun.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef __HV_VCPU__
#define __HV_VCPU__

#include <hv_arch_vcpu.h>
#include <stdint.h>
#include <kernel/thread.h>
#include <lk/console_cmd.h>

enum vcpu_mode {
	ARM_32 = 1,
	ARM_64,
};

enum vcpu_status {
	VCPU_CREATED = 0,
	VCPU_READY,
	VCPU_RUNNING,
	VCPU_BLOCKED,
	VCPU_SUSPEND,             /* low power, wait for interrupt  */
	VCPU_SHUTDOWN,            /* power off  */
	VCPU_DESTROY,             
	VCPU_INITF,
};

#define VCPU_FLAGS_EVENT (uint32_t)(1 << 0)   /*  have some events  */
#define VCPU_DEFAULT_STACK_SIZE  512 * 1024   /*  512k stack size  */

struct lk_vcpu {
	uint32_t id;
	uint32_t processor;                   /* the process this vcpu is running on  */
	short is_primary;                     /* whether worked as primary vcpu */
	short is_inited;                      /* finish inited  */   
	void *entry;
	void *args;
	enum vcpu_mode mode;
	enum vcpu_status status;
	struct lk_vm *vm;
	thread_t *thread;                    /* thread for this cpu */
	struct arch_vcpu arch_vcpu;
	int affinity;                        /* just a pinned cpu No. lk does support affinity */
	uint32_t flags;                      /* flags */
	spin_lock_t lock;                    /* the lock protect vcpu  */
};

#define VCPU_FLAGS_SET(vcpu, fl)           (vcpu)->flags &= (fl)
#define VCPU_FLAGS_CLEAR(vcpu, flags)      (vcpu)->flags &= (~(fl))

int hv_vm_init_vcpu(struct lk_vm *vm);
void hv_vcpu_init(void);
int hv_vcpu_start(struct lk_vcpu *vcpu);
void hv_arch_vcpu_launch(void *entry, void *arg);
void hv_vcpu_arch_contex_switch(struct lk_vcpu *new_vcpu, struct lk_vcpu *old_vcpu);
void hv_vcpu_arch_init(struct arch_vcpu *vcpu);
struct lk_vcpu *current_vcpu(void);
int hv_vcpu_stop(struct lk_vcpu *vcpu);
int hv_vcpu_block(struct lk_vcpu *vcpu);
int hv_vcpu_poweroff(struct lk_vcpu *vcpu);
int is_vcpu_poweroff(struct lk_vcpu *vcpu);
int is_vcpu_lowpower(struct lk_vcpu *vcpu);
int hv_vcpu_resume(struct lk_vcpu *vcpu);
struct lk_vcpu *hv_get_vcpu(struct lk_vm *vm, int32_t vcpu_id);
int hv_migrate_virq(struct lk_vcpu *o_vcpu, struct lk_vcpu *n_vcpu, struct vgic_irq *v_irq);
int hv_vcpu_cpu_id(struct lk_vcpu *vcpu);
int is_vcpu_running(struct lk_vcpu *vcpu);
int hv_poweroff_current(struct lk_vcpu *vcpu);
void vcpu_switch_to(struct lk_vcpu *vcpu);
#endif 
