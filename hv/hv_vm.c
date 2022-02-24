/*  vm management 
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

#include <hv_vm.h>
#include <lk/list.h>
#include <stdlib.h>
#include <lk/err.h>
#include <lk/trace.h>
#include <lib/console.h>
#include <stdio.h>
#include <kernel/mutex.h>
#include <string.h>
#include <hv_image.h>
#include <hv_vcpu.h>
#include <kernel/thread.h>
#include <hv_event.h>
#include <hv_arch_irq.h>

#define LOCAL_TRACE 1

/*TODO: memory pool for vm managment */
struct list_node vm_list_head;
mutex_t vm_lock;

/* allocat a vm structure */
struct lk_vm *lkhv_alloc_vm(void)
{
	struct lk_vm *vm = NULL;

	vm = malloc(sizeof(*vm) + sizeof(*(vm->info)));
	if (vm == NULL)
		return NULL;

	memset(vm, 0, sizeof(*vm) + sizeof(*(vm->info)));
	vm->info = (struct vm_info*)(vm + 1);
	return vm;
}

/* vm init:init lock and list  */
void hv_vm_init(void)
{
	list_initialize(&vm_list_head);
	mutex_init(&vm_lock);
}

/* free a vm structure  */
void lkhv_free_vm(struct lk_vm *vm)
{
	free(vm);
}

/* load mv into memory */
static int lkhv_vm_load(struct lk_vm *vm)
{
	int ret = 0;

	/* already load: derectly return  */
	if (vm->is_load)
		return 0;

	switch (vm->info->image_type) {
		case IMAGE_ELF:
			ret = vm_load_elf(vm);
			if (ret != NO_ERROR)
				break;
			
			ret = vm_load_fdt(vm);
			break;

		case IMAGE_FIT:
			ret = vm_load_fit(vm);
			break;

		default:
			ret = ERR_NOT_SUPPORTED;
			break;
	}

	if (ret == 0)
		vm->is_load = 1;

	return ret;
}

int lkhv_vm_init(struct lk_vm *vm)
{
	int ret = 0;

	/* allocation memory for vm  */
	LTRACEF("alloc memory for vm %s\n", vm->info->name);
	ret = lkhv_alloc_mem(vm);
	if (ret < 0 ) {
		TRACEF("faield to allocate memory for %s \n", vm->info->name);
		return ERR_NO_MEMORY;
	}	
	
	/*  init ram  allocat and map ram into stage2 page table 
	 *  first map than load image */
	ret = lkhv_map_ram(vm); 
	LTRACEF("map memory for vm %s\n", vm->info->name);
	if (0 > ret) {
		TRACEF("faield to map vm for %s \n", vm->info->name);
		goto map_failed;
	}
	
	lkhv_map_test_serial(vm);
	
	/* load vm image and related */
	LTRACEF("load image for vm %s\n", vm->info->name);
	ret = lkhv_vm_load(vm);
	if (0 > ret) {
		TRACEF("faield to load vm image for %s \n", vm->info->name);
		goto load_failed;
	}
	
	/* vm mmio space init */
	hv_vm_mmio_init(vm);
	
	/*  IRQ */
	ret = hv_vm_irq_init(vm);
	if (0 > ret) {
		TRACEF("faield to init irq for vm %s \n", vm->info->name);
		goto load_failed;
	}
	
	/*  TODO : device init */
	return 0;

load_failed:
	lkhv_unmap_ram(&(vm->mem));

map_failed:
	lkhv_vm_free_mem(vm);
	return ret;
}

/* free resource allocated for vm   */
void lkhv_vm_deinit(struct lk_vm *vm)
{
	/*  ummap vm memory   */
	lkhv_unmap_ram(&(vm->mem));

	/*  free vm memory  */
	lkhv_vm_free_mem(vm);

	/*  TODO : device deinit */
	/*  TODO : IRQ */
	/*  TODO : others  */

	return;
}

/* add vm into vm list  */
static void vm_add(struct lk_vm *vm)
{
	mutex_acquire(&vm_lock);
	list_add_tail(&vm_list_head, &(vm->vm_node));
	mutex_release(&vm_lock);	
}

/* get a vm by name, used for test  */
struct lk_vm *lkhv_mv_get(char *name)
{
	struct lk_vm *vm = NULL;

	mutex_acquire(&vm_lock);
	list_for_every_entry(&vm_list_head, vm, struct lk_vm, vm_node) {
		if (0 == strcmp(name, vm->info->name)) {
			mutex_release(&vm_lock);	
			return vm;
		}
	}	
	mutex_release(&vm_lock);	
	
	return NULL;
}

/* dump vm information */
int lkhv_vm_show(int argc, const console_cmd_args *argv)
{
	struct lk_vm *vm = NULL;
	int i = 1;

	mutex_acquire(&vm_lock);
	list_for_every_entry(&vm_list_head, vm, struct lk_vm, vm_node) {
		int num = 0;
		struct lk_vcpu *vcpu;

		printf("vm %d %s type %d information:\n", i, vm->info->name, vm->info->os_type);
		printf("%10s:%-10s\n", "image", vm->info->image);
		printf("%10s:%-10s\n", "fdt", vm->info->fdt);
		printf("%10s:0x%-10d\n","image_type",vm->info->image_type);
		printf("%10s:0x%-10lx\n", "ram size", vm->info->ram_size);
		printf("%10s:0x%-10lx\n", "ipa start", vm->info->ipa_start);
		printf("vcpu information is :\n");
		for (num = 0; num < vm->vcpu_num; num++) {
			vcpu = vm->vcpu[num];
			printf("%d vcpu %p status %d\n", num, vcpu, vcpu->status);
		}

		i++;
	}	
	mutex_release(&vm_lock);
	return 0;	
}

/* init a vm  */
int lkhv_init_vm(struct lk_vm *vm)
{
	int ret = 0;

	list_initialize(&(vm->vm_node));
	
	/*  vcpu init */
	ret =  hv_vm_init_vcpu(vm);
	if (0 > ret) {
		TRACEF("faield to init vcpus for vm %s\n", vm->info->name);
		return -1;;
	}
	
	vm_add(vm);
	return 0;
}

/* start vm   */
int lkhv_vm_start(struct lk_vm *vm)
{
	int ret;
	struct lk_vcpu *vcpu;
	
	LTRACEF(" find primary vcpu in vm %s\n", vm->info->name);
	vcpu = vm->vcpu[vm->primary_cpu];
	LTRACEF("vcpu %p primary %d\n", vcpu, vcpu->is_primary);
	ret = hv_vcpu_start(vcpu);
	
	return ret;
}

/* start a vm  */
int lkhv_start_vm(char *vm_name)
{
	struct lk_vm *vm = NULL;

	vm = lkhv_mv_get(vm_name);
	if (NULL == vm) {
		TRACEF("failed to get vm %s\n", vm_name);
		return ERR_NOT_FOUND;
	}

	return lkhv_vm_start(vm);
}

int vm_start(int argc, const console_cmd_args *argv)
{
	char *name = NULL;
	
	if (argc < 2) {
		TRACEF(" lacks of args\n");
		return ERR_INVALID_ARGS;
	}


	name = (char *)argv[1].str;
	LTRACEF("will start vm %s\n", name);
	return lkhv_start_vm(name);
}

/* shutdown a vm  */
int lkhv_shutdown_vm(struct lk_vm *vm)
{
	int ret;
	struct lk_vcpu *vcpu = NULL;

	vcpu = current_vcpu();
	if (NULL == vcpu)
		return ERR_NOT_FOUND;
	
	/* check whether is already poweroff  */
	if (VCPU_SHUTDOWN == vcpu->status)
		return 0;
	
	{
		/* reseting vm resources such as io/timer/irq......  */
		int i;
		struct lk_vcpu *o_vcpu;
		int sched = 0;

		for (i = 0; i < vm->vcpu_num; i++) {
			o_vcpu = vm->vcpu[i];
			if (o_vcpu == vcpu)   /* see comments below  */
				continue;
			ret = hv_vcpu_poweroff(o_vcpu);
			if (ret < 0) {
				TRACEF("failed to poweroff vcpu %d\n", i);
				return ret;
			}

			if (ret == VCPU_RESCHED_EXIT)
				sched = 1;	
		}
	
		/* we need to power off currrent vcpu last, 
		 * or else it will poweroff only one vcpu, 
		 * because it in current vcpu context currently   */	
		ret = hv_vcpu_poweroff(vcpu);
		if (ret < 0) {
			TRACEF("failed to poweroff vcpu in vm %s\n", vm->info->name);
			return ret;
		}
		
		if (ret == VCPU_RESCHED_EXIT)
			sched = 1;
		
		/* last free vm resources  */
		lkhv_vm_free_mem(vm);

		if (sched) {
			THREAD_LOCK(state);
			thread_block();
			THREAD_UNLOCK(state);
		}
	}

	return 0;
}

/* TODO delete a vm */
int lkhv_delete_vm(int vm_id)
{
	return 0;	
}

STATIC_COMMAND_START
STATIC_COMMAND("vm_show", "show vm information", &lkhv_vm_show)
STATIC_COMMAND("vm_start", "start a vm", &vm_start)
STATIC_COMMAND("t_event", "test hv event", &hv_event_test)
STATIC_COMMAND_END(vmctl);
