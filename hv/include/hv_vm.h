/* @author pwang
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

#ifndef __HV_VM__
#define __HV_VM__

#include <lk/list.h>
#include <hv_def.h>
#include <hv_mem.h>
#include <lk/console_cmd.h>
#include <hv_mmio.h>

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({                      \
	const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
	(type *)( (char *)__mptr - offsetof(type,member) );})

/* memory allocation  */
struct mem_2m_blocks {
	unsigned long start;
	struct list_node node;
};

enum vm_os_type {
	OS_LINUX = 1,
	OS_LK,
	OS_END,
};

enum vm_image_type {
	IMAGE_ELF = 1,
	IMAGE_FIT = 2,
};

/* vm information */
struct vm_info {
	char name[HV_BUFFER_MIN_LEN];
	char image[HV_BUFFER_MIN_LEN];     /* image name  */
	char fdt[HV_BUFFER_MIN_LEN];       /* dtb */
	int  os_type;                      /* os_type  */
	int  image_type;                   /* image_type: elf or fit  */
	int  priority;
	int  timeslice;
	int  auto_boot;                    /* is it auto boot  */
	unsigned long entry;               /* entry */
	unsigned long args;                /* where to load dtb */
	unsigned long ipa_start;           /* ram start address */              
	unsigned long ram_size;	           /* ram size, should be 2M aligment */
	int vcpu_num;                      /* vcpu num */
	int vcpu_mode;                     /* vcpu mode */
	int *vcpu_affinity;                /* vcpu affinity array */
	void *vm_cfg;                      /* vm dtb config */
	uint32_t cfg_offset;            /* vm dtb offset */
};

struct lk_vm {
	uint32_t vmid;
	struct vm_info *info;
	struct list_node mem_head;         /* allocation information */
	struct list_node vm_node;
	int is_load;                       /* whether we already load image */	
	struct vm_mem mem;                 /* memory related  */
	int vcpu_num;
	int primary_cpu;                   /* which one is the primary cpu */
	struct lk_vcpu **vcpu;             /* vcpu pointer array  */
	void *interrupt;                   /* vm interrupt  */
	struct mmio_space mmio;            /* vm mmio space  */
	int dev_num;                       /* device information */
	struct vm_dev* devs;     	
	/* others timers、device and so on */
};

struct lk_vm *lkhv_alloc_vm(void);
void lkhv_free_vm(struct lk_vm *vm);
int lkhv_init_vm(struct lk_vm *vm);
void hv_vm_init(void);
int lkhv_alloc_mem(struct lk_vm *vm);
void lkhv_mem_init(void);
int lkhv_map_ram(struct lk_vm *vm); 
void lkhv_vm_free_mem(struct lk_vm *vm);
int lkhv_vm_show(int argc, const console_cmd_args *argv);
struct lk_vm *lkhv_mv_get(char *name);
int lkhv_vm_start(struct lk_vm *vm);
int lkhv_shutdown_vm(struct lk_vm *vm);
void lkhv_vm_deinit(struct lk_vm *vm);
int lkhv_vm_init(struct lk_vm *vm);
int lkhv_map_test_serial(struct lk_vm *vm);
int lkhv_start_vm(char *vm_name);
#endif 
