/*  hv event 
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
#include <hv_mem.h>
#include <lib/cbuf.h>
#include <platform/interrupts.h>
#include <hv_event.h>
#include <hv_vcpu.h>
#include <hv_vm.h>
#include <arch/arch_ops.h>
#include <arch/mp.h>
#include <kernel/mp.h>
#include <lk/pow2.h>
#include <arch/ops.h>
#include <lk/trace.h>
#include <lk/console_cmd.h>

#define LOCAL_TRACE 0

#define PERCPU_MAX_EVENT  32
/* TODO:  it is better to handle event in thread context  */

struct cpu_event {
	cbuf_t  e_buff;
}__ALIGNED(CACHE_LINE);

static struct cpu_event cpu_event[SMP_MAX_CPUS];

typedef int (*event_cb)(void *arg);
static int event_cb_invalid(void *arg)
{
	printf("invalid event\n");
	return 0;
};

static int e_cb_vcpu_stop(void *arg)
{
	struct lk_vcpu *vcpu = *((struct lk_vcpu **)arg);
	return hv_vcpu_stop(vcpu);
}

static int e_cb_vcpu_start(void *arg)
{
	struct lk_vcpu *vcpu = *((struct lk_vcpu **)arg);
	return hv_vcpu_start(vcpu);
}

static int e_cb_vcpu_poweroff(void *arg)
{
	struct lk_vcpu *vcpu = *((struct lk_vcpu **)arg);
	return hv_vcpu_poweroff(vcpu);
}

static int e_cb_vm_shutdown(void *arg)
{
	struct lk_vcpu *vcpu = *((struct lk_vcpu **)arg);
	return lkhv_shutdown_vm(vcpu->vm);
}

static event_cb e_cb[HV_EVENT_VCPU_MAX] = {
	&event_cb_invalid,
	(event_cb)e_cb_vcpu_stop,
	(event_cb)e_cb_vcpu_start,
	(event_cb)e_cb_vcpu_poweroff,
	(event_cb)e_cb_vcpu_start,
	(event_cb)e_cb_vm_shutdown,
};

/* event handler, we need to register it in other places  */
/* call it with interrupt disabled */
enum handler_return hv_event_handler(void *args)
{
	size_t len;
	struct cpu_event *evt;
	struct hv_event h_evt;
	event_cb cb_func = NULL;

	evt = cpu_event + arch_curr_cpu_num();
	smp_rmb();
	len = cbuf_read(&(evt->e_buff), &h_evt, sizeof(h_evt), 0);
	if (len != sizeof(h_evt)) {
		TRACEF("failed to recv event from buf len %ld on cpu %u\n", 
				len, arch_curr_cpu_num());
		goto out;
	}

	LTRACEF("recv event %d on cpu %u, %p\n",
		       	h_evt.e_id, arch_curr_cpu_num(), h_evt.buf);
	cb_func = e_cb[h_evt.e_id];
	return cb_func((void *)h_evt.buf);

out:
	return INT_NO_RESCHEDULE;
}

/* just an empty handler  */
enum handler_return hv_event_empty_handler(void *args)
{
	return INT_NO_RESCHEDULE;
}

/* hv event init  */
int hv_event_init(void)
{
	int i = 0;
	struct cpu_event *evt;
	int len = PERCPU_MAX_EVENT * sizeof(struct hv_event);

	len = round_up_pow2_u32(len);
	/* init buffer for each cpu it's better to init during each cpu startup */
	for (i = 0; i < SMP_MAX_CPUS; i++) {
		evt = cpu_event + i;
		LTRACEF("init cpu buffern %d\n", len);
		cbuf_initialize(&(evt->e_buff), len);
	}

	return 0;
}

/* send an event to dest cpu  
 * cpu is the dest cpu  */
int hv_send_event(uint32_t cpu, struct hv_event *evt)
{
	size_t len;
	struct cpu_event *c_evt;

	c_evt = cpu_event + cpu;
	len = cbuf_write(&(c_evt->e_buff), evt, sizeof(*evt), 0);
	if (len != sizeof(*evt)) {
		TRACEF("failed to write event to  buf, len %ld\n", len);
		return ERR_NO_MEMORY;
	}
	
	/* flush cache into memory */
	smp_wmb();

	LTRACEF("send event %d to cpu %d\n", evt->e_id, cpu);
	arch_mp_send_ipi(1U << cpu, MP_IPI_HV_EVENT);

	return 0;
}

/* send an empyt event to target cpu  */
void  hv_empty_event(uint32_t cpu)
{
	LTRACEF("send empty event  to cpu %d\n", cpu);
	arch_mp_send_ipi(1U << cpu, MP_IPI_HV_EVENT1);
}

int hv_event_test(int argc, const console_cmd_args *argv)
{
	int e_id = 0;
	int d_cpu = 0;
	struct hv_event evt;
	
	if (argc < 3) {
		TRACEF(" lacks of args\n");
		return ERR_INVALID_ARGS;
	}


	e_id = (int)argv[1].u;
	d_cpu = (int)argv[2].u;
	printf("will send event %d to cpu %d\n", e_id, d_cpu);
	memset(&evt, 0, sizeof(evt));
	evt.e_id = e_id;
	return hv_send_event(d_cpu, &evt);
}
