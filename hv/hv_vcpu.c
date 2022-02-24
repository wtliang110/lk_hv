/*  vcpu related functions 
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

#include <lk/list.h>
#include <stdlib.h>
#include <lk/err.h>
#include <lk/trace.h>
#include <stdio.h>
#include <kernel/mutex.h>
#include <string.h>
#include <hv_vcpu.h>
#include <kernel/thread.h>
#include <hv_mem.h>
#include <hv_vm.h>
#include <kernel/thread.h>
#include <hv_event.h>
#include <hv_vtimer.h>

#define LOCAL_TRACE 1

struct vcpu_percpu {
	struct lk_vcpu *vcpu;
}__ALIGNED(CACHE_LINE);

static struct vcpu_percpu vcpu_data[SMP_MAX_CPUS];
static struct vcpu_percpu *current_vcpu_data(void) {
	return vcpu_data + arch_curr_cpu_num();
}

/* need first memset 0 for vcpu_date */
void hv_vcpu_init(void)
{
	memset(vcpu_data, 0, sizeof(vcpu_data));
}

struct lk_vcpu *current_vcpu(void)
{
	return (current_vcpu_data())->vcpu;
}

#define VCPU_LOCK(vcpu)   spin_lock_irqsave(&vcpu->lock, vcpu_state)
#define VCPU_UNLOCK(vcpu) spin_unlock_irqrestore(&vcpu->lock, vcpu_state)

/* vcpu context switch, need interrupt disable */
void vcpu_switch_to(struct lk_vcpu *vcpu)
{
	struct lk_vcpu *old_vcpu = NULL;
	struct vcpu_percpu *vcpu_curr = NULL;

	vcpu_curr = current_vcpu_data();
	old_vcpu =  vcpu_curr->vcpu;
	if (old_vcpu == vcpu ||
		NULL == vcpu ||
		0 == vcpu->is_inited)                  /* same cpu, do not need to switch  */
		return;
	
	hv_vcpu_arch_contex_switch(vcpu, old_vcpu);
	vcpu_curr->vcpu = vcpu;
	LTRACEF("switch to vcpu %p\n", vcpu);
		
	return;
}

/* 
 * smp support, need to emulate smc for guest 
 * firt create a simple one, support for test 
 * how primary start secondary */
static int vcpu_start_common(struct lk_vcpu *vcpu)
{
	LTRACEF("start vcpu %p\n", vcpu);
	
	/* archtecture init  */	
	hv_vcpu_arch_init(&(vcpu->arch_vcpu));
	
	/*  interrupt init */
	hv_vcpu_irq_init(vcpu);

	/*  timer init  */
	hv_vcpu_vtimer_init(vcpu);
	
	vcpu->is_inited = 1;
	vcpu_switch_to(vcpu);
	LTRACEF("vcpu %p will launch %p\n", vcpu, vcpu->entry);
	hv_arch_vcpu_launch(vcpu->entry, vcpu->args);

	/* will never come here */
	return 0;
}

/*  primary cpu entry, which will init vm resoures  */
static int vcpu_entry_primary(void *arg)
{
	int ret = 0;
	struct lk_vcpu *vcpu = (struct lk_vcpu *)(arg);
	struct lk_vm *vm = vcpu->vm;

	ret = lkhv_vm_init(vm);
	if (0 > ret) {
		TRACEF("faield to start vm %s \n", vm->info->name);
		return -1;
	}
	
	/*start vcpu*/
	LTRACEF("primary vcpu %p will entry its world\n", vm->info->name);
	vcpu->entry = (void *)vm->info->entry;
	vcpu->args = (void *)vm->info->args;
	ret = vcpu_start_common(vcpu);
	if (0 > ret) {
		TRACEF("faield to start vcpu %s \n", vm->info->name);
		goto start_failed;
	}
	
	return 0;	

start_failed:
	lkhv_vm_deinit(vm);
	vcpu->status = VCPU_INITF;
	return ret;
}

/* second cpu entry  */
static int vcpu_entry_secondary(void *arg)
{
	int ret = 0;
	struct lk_vcpu *vcpu = (struct lk_vcpu *)(arg);

	ret = vcpu_start_common(vcpu);
	if (0 > ret) 
		TRACEF("failed to start vcpu %p\n", vcpu);

	return ret;
}

/* start a vcpu  */
int hv_vcpu_start(struct lk_vcpu *vcpu)
{
	thread_t *t = NULL;
	spin_lock_saved_state_t vcpu_state;

	LTRACEF("hv_vcpu_start iii %p\n", vcpu);

	/* protect that only one context  to call this function  */
	VCPU_LOCK(vcpu);
	if (VCPU_CREATED != vcpu->status &&
	    VCPU_SHUTDOWN != vcpu->status) {
		VCPU_UNLOCK(vcpu);
		return ERR_BUSY;
	}

	/*  we use thread_block to shutdown a vcpu, 
	 *  which will change thread state inot blocked, 
	 *  but thread_resume requred thread state is suspended
	 *  maybe we need a specific thread api for vcpu power off  */
	t = vcpu->thread;
	LTRACEF("resume vcpu %p thread on cpu %d %d, current cpu is %d\n", 
		vcpu, t->curr_cpu, t->pinned_cpu, arch_curr_cpu_num());
	if (VCPU_SHUTDOWN == vcpu->status) {
		THREAD_LOCK(state);
		if (t->state == THREAD_BLOCKED)
			t->state = THREAD_SUSPENDED;
		THREAD_UNLOCK(state);

		/*  clear thread resources before resume it */
		thread_reset(t);
	}	
	
	vcpu->status = VCPU_READY;
	VCPU_UNLOCK(vcpu);

	thread_resume(vcpu->thread);
	return 0;
}

/* stop the thread on current cpu
 * return value:
 * 1: means after stop, need to resched,
 * caller should be make sure @thd is on current cpu */
static int stop_current_cpu_thread(thread_t *thd)
{
	int ret = 0;

	THREAD_LOCK(state);
	switch (thd->state) {
		case THREAD_READY:
			thd->state = THREAD_BLOCKED;
			remove_task_from_rqueue(thd); 
			break;

		case THREAD_RUNNING:
			thd->state = THREAD_BLOCKED;
			ret = 1;
			break;                               /*  will not come to here  */

		default:
			thd->state = THREAD_BLOCKED;
			break;
		}
		
	THREAD_UNLOCK(state);
	return ret;
}

/* block @vcpu, may suspend if vcpu is currently running vcpu */
int hv_vcpu_block(struct lk_vcpu *vcpu)
{
	int ret;
	spin_lock_saved_state_t vcpu_state;
	
	VCPU_LOCK(vcpu);
	ret = hv_vcpu_stop(vcpu);
	VCPU_UNLOCK(vcpu);
	if (ret == VCPU_RESCHED_EXIT)
		thread_block();

	return ret;
}

/* resume a blocked vcpu */
int hv_vcpu_resume(struct lk_vcpu *vcpu)
{
	thread_t *thd = vcpu->thread;
	spin_lock_saved_state_t vcpu_state;
	
	VCPU_LOCK(vcpu);
	if (VCPU_BLOCKED != vcpu->status) {
		VCPU_UNLOCK(vcpu);
		return -1;
	} 
	vcpu->status = VCPU_READY;
	VCPU_UNLOCK(vcpu);

	THREAD_LOCK(state);
	thd->state = THREAD_READY;
	THREAD_UNLOCK(state);
	thread_resume(thd);
	return 0;
}

/*  directly operate on thread */
/*  suspend a vcpu  */
/*  return resched, the caller need to resched   */
int hv_vcpu_stop(struct lk_vcpu *vcpu)
{
	int ret = 0;
	uint32_t t_cpu = -1;
	thread_t *thd = vcpu->thread;

	/* check whether is already poweroff  */
	if (VCPU_BLOCKED == vcpu->status)
		return ret;

	t_cpu = hv_vcpu_cpu_id(vcpu);
	if ((int)t_cpu < 0) {
		TRACEF("failed to get cpuid for vcpu %p\n", vcpu);
		return 0;
	}

	/* need to check vcpu status   */
	if (t_cpu == arch_curr_cpu_num()) {
		vcpu->status = VCPU_BLOCKED; 		
		if (stop_current_cpu_thread(thd)) 
			return VCPU_RESCHED_EXIT;
		else 
			return 0;
	} else {
		/* send event to stop thread */
		struct hv_event evt;

		evt.e_id = HV_EVENT_VCPU_STOP;
		*((struct lk_vcpu **)(evt.buf + 0)) = vcpu;	
		ret = hv_send_event(t_cpu, &evt);
		if (ret != 0) 
			TRACEF("failed to send event vcpu stop\n");
		return ret;
	}

	return 0;
}

/* whether vcpu is poweroff  */
int is_vcpu_poweroff(struct lk_vcpu *vcpu)
{
	return vcpu->status == VCPU_SHUTDOWN;
}

/* whether vcpu is lowerpower  */
int is_vcpu_lowpower(struct lk_vcpu *vcpu)
{
	return vcpu->flags & VCPU_FLAGS_EVENT;
}

/* is vcpu is running  */
int is_vcpu_running(struct lk_vcpu *vcpu)
{
	return vcpu->status == VCPU_RUNNING;
}

/* get cpu id, which the vcpu is running on */
int hv_vcpu_cpu_id(struct lk_vcpu *vcpu)
{
	int32_t t_cpu;
	thread_t *thd = vcpu->thread;

	if (thd->pinned_cpu != -1)
		t_cpu = thd->pinned_cpu;
	else if (thd->curr_cpu != -1)
		t_cpu = thd->curr_cpu;
	else
		t_cpu = -1;

	return t_cpu;
}

struct lk_vcpu *hv_get_vcpu(struct lk_vm *vm, int32_t vcpu_id)
{
	if (vcpu_id >= vm->vcpu_num)
		return NULL;
	return vm->vcpu[vcpu_id];
}

int hv_poweroff_current(struct lk_vcpu *vcpu)
{
	thread_t *thd = vcpu->thread;
	int resched = 0;
		
	LTRACEF("vcpu %p in current\n", vcpu);	
	/* need to stop vcpu first  */
	vcpu->status = VCPU_SHUTDOWN; 		
	resched = stop_current_cpu_thread(thd);
#if 0
	ret = thread_reset(thd);
	if (ret != 0) {
		TRACEF("failed to reset thread\n");
		return -1;
	}
#endif
		
	/*  TODO: others init such as interrupt  */
	if (resched) {
		return VCPU_RESCHED_EXIT;
	} else
		return 0;	
}

/* power off vcpu  */
int hv_vcpu_poweroff(struct lk_vcpu *vcpu)
{
	int ret = 0;
	uint32_t t_cpu = -1;

	/* check whether is already poweroff  */
	if (VCPU_SHUTDOWN == vcpu->status ||
		VCPU_CREATED == vcpu->status)
		return ret;
	
	t_cpu = hv_vcpu_cpu_id(vcpu);
	if ((int)t_cpu < 0) {
		TRACEF("failed to get cpu id for vcpu %p", vcpu);
		return 0;
	}

	LTRACEF("will power off vcpu %p\n", vcpu);
	/* need to check vcpu status   */
	if (t_cpu  == arch_curr_cpu_num()) {
		/* cleanup resources  */
		
		/* irq cleanup  */
		hv_vcpu_irq_cleanup(vcpu);
		return hv_poweroff_current(vcpu);
	} else {
		/* send event to stop thread */
		struct hv_event evt;

		evt.e_id = HV_EVENT_VCPU_POWEROFF;
		*((struct lk_vcpu **)(evt.buf + 0)) = vcpu;	
		LTRACEF("send event to power offer vcpu %p on cpu %u\n", vcpu, t_cpu);
		ret = hv_send_event(t_cpu, &evt);	
		if (ret != 0) 
			TRACEF("failed to send event vcpu power off\n");
		return 0;
	}

	return ret;
}

/* @id is the logical vcpu id; 
 * @is_primary: whether this vcpu is the primary core,
 * the first one is the primary cpu  */
/* lk thread state 
 * ------create---->THREAD_SUSPENDED 
 *                        |
 *                        |   +--------------------<<<<<<<----wake_up----------------+
 *                     resume |                                                      |
 *                        |   |                                                      |
 *                        V   V                                                      |
 *                 THREAD_READY<-----yield-------THREAD_RUNNING-----lock:wait--->THREAD_BLOCKED
 *                        |                           A   |
 *                        |                           |   |
 *                        |                           |   |
 *                        +-----------sched-----------+   +---exit------>THREAD_DEATH
 * 
 * */
static int hv_vcpu_create(struct lk_vm *vm, int id)
{
	int ret;
	struct vm_info *info = NULL; 
	struct lk_vcpu *vcpu = NULL;
	char name[HV_BUFFER_MIN_LEN * 2] = { 0 };

	info = vm->info;
		
	vcpu = malloc(sizeof(*vcpu));
	if (NULL == vcpu) {
		TRACEF("failed to allocate vcpu structure\n");
		return ERR_NO_MEMORY;
	}
	memset(vcpu, 0, sizeof(*vcpu));

	vcpu->id = id;
	vcpu->is_primary = (id == 0);
	if (vcpu->is_primary)
		vm->primary_cpu = id;

	LTRACEF("vcpu %d mode is %d primary %d\n", id, info->vcpu_mode, vcpu->is_primary);
	if (64 == info->vcpu_mode) 
		vcpu->mode = ARM_64; 
	else
		vcpu->mode = ARM_32; 

	vcpu->vm = vm;
	snprintf(name, sizeof(name) - 1, "%s_vcpu_%d", 
			info->name, id);    
	vcpu->thread = thread_create(name, 
	                vcpu->is_primary ? vcpu_entry_primary : vcpu_entry_secondary,
       		 	vcpu, info->priority, VCPU_DEFAULT_STACK_SIZE);
	if (vcpu->thread == NULL) {
		TRACEF("failed to create thread for thread %s\n", name);
		ret = ERR_NOT_READY;
		goto failed;
	}
	LTRACEF("created thread %s for vcpu %d\n", name, id);
	vcpu->thread->vcpu = vcpu;
	
	vcpu->affinity = info->vcpu_affinity[id];
	if (0 <= vcpu->affinity && SMP_MAX_CPUS > vcpu->affinity)
		thread_set_pinned_cpu(vcpu->thread, vcpu->affinity);

	LTRACEF("vcpu %d target cpu is %d\n", id, vcpu->affinity);
	hv_vcpu_arch_init(&(vcpu->arch_vcpu));
	vm->vcpu[id] = vcpu;			
	vcpu->status = VCPU_CREATED;
	vcpu->lock = SPIN_LOCK_INITIAL_VALUE;
	LTRACEF("add vcpu %p to vm\n", vcpu);
	
	/* thread resume  */
	if (vm->info->auto_boot) {
		LTRACEF("vcpu %p is auto boot\n", vcpu);
		hv_vcpu_start(vcpu);
	}
	
	return 0;

failed:
	free(vcpu);
	return ret;
}

static void hv_vcpu_free(struct lk_vcpu *vcpu)
{
	free(vcpu);
	return;
}

/* init vcpus of vm @vm  */
int hv_vm_init_vcpu(struct lk_vm *vm)
{
	int i;
	int vcpu_num;
	int ret = 0;

	vcpu_num = vm->info->vcpu_num;	
	LTRACEF("vm %s has %d vcpu\n", vm->info->name, vcpu_num);

	vm->vcpu = malloc(sizeof(struct lk_vcpu *) * vcpu_num);
	if (NULL == vm->vcpu) {
		TRACEF("failed to allocate memory for vcpu pointers\n");
		return ERR_NO_MEMORY;
	}
	memset(vm->vcpu, 0, sizeof(struct lk_vcpu *) * vcpu_num);

	for (i = 0; i < vcpu_num; i++) {
		LTRACEF("create vcpu %d\n", i);
		ret = hv_vcpu_create(vm, i);
		if (ret != NO_ERROR) {
			TRACEF("failed to create %d vcpu\n", i);
			goto failed;
		}
	}

	vm->vcpu_num = vcpu_num;
	return 0;

failed:
	for (i = 0; i < vcpu_num; i++) {
		struct lk_vcpu *vcpu = NULL;
		
		vcpu = vm->vcpu[i];
		if (NULL != vcpu)
			hv_vcpu_free(vcpu);
	}

	free(vm->vcpu);
	return ret;
}
