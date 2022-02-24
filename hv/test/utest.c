/* this is the test code for hv  */
#include <lk/err.h>
#include <hv_vm.h>
#include <hv_mem.h>
#include <stdio.h>
#include <kernel/vm.h>
#include <lk/console_cmd.h>
#include <vm_config.h>
#include <hv_irq.h>
#include <hv_vgic.h>
#include <dev/interrupt/arm_gic.h>
#include <vgic_gic.h>
#include <kernel/thread.h>

/* hv functions test case  */
int hv_test(int argc, const console_cmd_args *argv)
{
	int i = 0, l = 0;
	char *vm_name = NULL;
	int ret;
	paddr_t ipa, pa, r_pa;
	struct lk_vm *vm = NULL;
	struct vm_mem *vm_mem = NULL;
	struct phy_mem_seg *mem_seg = NULL;


	if (2 > argc) {
		printf("lack of args\n");
		return ERR_INVALID_ARGS;
	}

	vm_name = (char *)(argv[1].str);
	vm = lkhv_mv_get(vm_name);
	if (NULL == vm) 
		printf("failed to find vm %s\n", vm_name);
	else
		printf("will test vm %s %p\n", vm_name, vm);
	
	ret = lkhv_alloc_mem(vm);
	if (ret < 0 ) {
		printf("faield to allocate memory for %s \n", vm->info->name);
		return ERR_NO_MEMORY;
	}	
	
	/*  init ram  allocat and map ram into stage2 page table 
	 *  first map than load image */
	ret = lkhv_map_ram(vm); 
	printf("map memory for vm %s\n", vm->info->name);
	if (0 > ret) {
		printf("faield to map vm for %s \n", vm->info->name);
		return -1;
	}

	/* memory test */
	vm_mem = &(vm->mem);
	printf("memory info is :\n");
	for (i = 0; i < vm_mem->seg_num; i++) {
		mem_seg = vm_mem->mem_seg + i;
		printf("ipa 0x%lx pa 0x%lx\n", mem_seg->ipa, mem_seg->pa);
	}

	mem_seg = vm_mem->mem_seg;
	printf("traslate ipa to pa\n");
	for (l = 0; l < 5; l++) {
		ipa = mem_seg->ipa + VM_MEM_BLOCK_2M * l; 
		r_pa = mem_seg->pa + VM_MEM_BLOCK_2M * l;
		for (i = 0; i < 5; i++) {
			ipa = ipa + PAGE_SIZE * i;
			r_pa = r_pa + PAGE_SIZE *i;
			pa = ipa_to_pa(&(vm_mem->arch_mem), ipa);
			printf("ipa 0x%lx----->pa 0x%lx : rpa 0x%lx %s\n",
				       	ipa, pa, r_pa,
					(pa == r_pa) ? "pass" : "fail");
		}
	}
	return 0;
}

/* hv functions test case  */
int hv_irq_test(int argc, const console_cmd_args *argv)
{
	char *vm_name = NULL;
	int ret;
	struct lk_vm *vm = NULL;
	int virq, irq;
	struct vgic_irq *p_irq;


	if (2 > argc) {
		printf("lack of args\n");
		return ERR_INVALID_ARGS;
	}

	vm_name = (char *)(argv[1].str);
	vm = lkhv_mv_get(vm_name);
	if (NULL == vm) 
		printf("failed to find vm %s\n", vm_name);
	else
		printf("will test vm %s %p\n", vm_name, vm);

	ret = lkhv_vm_init(vm);
	if (ret != 0) {
		printf("failed to init vm");
		return 0;
	}

	hv_vcpu_irq_init(vm->vcpu[0]);

	virq = 10, irq = 10;
	ret = hv_map_irq_to_guest(vm, virq, irq);
	if (ret != ERR_NOT_FOUND) {
		printf("test failed virq %u, irq %u\n", virq, irq);
		return 0;
	}

	virq = 35, irq = 35;
	ret = hv_map_irq_to_guest(vm, virq, irq);
	if (ret != 0) {
		printf("test failed virq %u, irq %u ret %d\n", virq, irq, ret);
		return 0;
	}

	printf("will inject virq %u irq %u\n", virq, irq);
	p_irq = get_virq(vm, NULL, virq);
       	if (NULL == p_irq) {
		printf("failed to get pirq for %d\n", virq);
		return 0;
	}
			
	hv_virq_enable(p_irq, 1);
	//gic_set_enable(35, 1);
	//hv_int_handler(irq);
	gic_trigger_interrupt(35);	
	printf("trigger pirq %d\n", irq);	
	hv_flush_queued_virqs(vm->vcpu[0]);	

	printf("lr for %d is %llx\n", 0, vgic_read_lr_reg(0));
	return 0;
}

int hv_vm_irq_test(int argc, const console_cmd_args *argv)
{
	char *vm_name = NULL;
	int ret;
	struct lk_vm *vm = NULL;
	int virq, irq;
	struct vgic_irq *p_irq;


	if (2 > argc) {
		printf("lack of args\n");
		return ERR_INVALID_ARGS;
	}

	vm_name = (char *)(argv[1].str);
	vm = lkhv_mv_get(vm_name);
	if (NULL == vm) 
		printf("failed to find vm %s\n", vm_name);
	else
		printf("will test vm %s %p\n", vm_name, vm);

	/* start vm  */
	ret = lkhv_start_vm(vm_name);
	if (0 > ret) {
		printf("failed to start vm %s ret %d\n", vm_name, ret);
		return 0;
	}	

	thread_sleep(1000);
	virq = 35, irq = 35;
	printf("map irq %d to vm %p\n", irq, vm);
	ret = hv_map_irq_to_guest(vm, virq, irq);
	if (ret != 0) {
		printf("test failed virq %u, irq %u ret %d\n", virq, irq, ret);
		return 0;
	}
	
	printf("trigger pirq %d\n", irq);		
	gic_trigger_interrupt(35);	
		
	return 0;
}

int hv_vm_sgi_test(int argc, const console_cmd_args *argv)
{
	char *vm_name = NULL;
	int ret;
	struct lk_vm *vm = NULL;
	int virq, irq;
	struct vgic_irq *p_irq;


	if (2 > argc) {
		printf("lack of args\n");
		return ERR_INVALID_ARGS;
	}

	vm_name = (char *)(argv[1].str);
	vm = lkhv_mv_get(vm_name);
	if (NULL == vm) 
		printf("failed to find vm %s\n", vm_name);
	else
		printf("will test vm %s %p\n", vm_name, vm);

	/* start vm  */
	ret = lkhv_start_vm(vm_name);
	if (0 > ret) {
		printf("failed to start vm %s ret %d\n", vm_name, ret);
		return 0;
	}	
	
	return 0;
}

STATIC_COMMAND_START
STATIC_COMMAND("hv_test", "hypervisor test", &hv_test)
STATIC_COMMAND("irq_test", "irq test", &hv_irq_test)
STATIC_COMMAND("irq_vm", "vm irq test", &hv_vm_irq_test)
STATIC_COMMAND("sgi_vm", "vm sgi test", &hv_vm_sgi_test)
STATIC_COMMAND_END(hv_test);
