/* root device to hold vm image and others 
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

#include <stdio.h>
#include <lib/fs.h>
#include <vm_config.h>
#include <string.h>
#include <lk/init.h>
#include <hv_def.h>
#include <hv_vm.h>
#include <hv_mem.h>
#include <vm_config.h>
#include <hv_arch_vcpu.h>
#include <hv_vcpu.h>
#include <sys/types.h>
#include <hv_event.h>
#include <hv_irq.h>
#include <hv_vtimer.h>
#include <lk/init.h>

/* our vmcfg configure located at root directory */
#define VMCFG_PATH "/vmcfg.dtb"

struct hv_bootarg {
	char bio[HV_BUFFER_MIN_LEN];
	char fs[HV_BUFFER_MIN_LEN];
};

static int parse_bootargs(char *bootarg, struct hv_bootarg *hv_boot_info)
{
	char *tmp = NULL;
	char *tmp1 = NULL;
	
	tmp = strstr(bootarg, "root=");
	if (NULL == tmp)
		return -1;

	tmp = tmp + 5;
	tmp1 = strchr(tmp, ',');
	if (tmp1 == NULL)
		return -2;

	strncpy(hv_boot_info->bio, tmp, tmp1 - tmp);
	tmp = tmp1 + 1;
	strncpy(hv_boot_info->fs, tmp, sizeof(hv_boot_info->fs) - 1);

	return 0;
}

static int hv_rootdev_init(void)
{
	int ret = 0;
	char *bootarg = NULL;
	int cfg_len = 0;
	struct hv_bootarg ba;
	
	bootarg = get_root_config(&cfg_len);
	if (NULL == bootarg)
		return -1;

	memset(&ba, 0, sizeof(ba));
	ret = parse_bootargs(bootarg, &ba);
	if (0 > ret) {
		printf("failed to parse args %d\n", ret);
		return ret;
	}

	printf("will mount %s fs %s to /\n", ba.bio, ba.fs);
	ret = fs_mount("/", ba.fs, ba.bio);
	if (0 > ret) {
		printf("failed to mount rootfs ret %d\n", ret);
		return ret;
	}

	return ret;
}

static void hv_boot_init(uint level)
{
	int ret = 0;

	ret = hv_rootdev_init();
        if (0 > ret) {
		printf("faield to init root dev\n");
		return;
	}
#if 0
	this need to init during each cpu, so remove it here 
	/* hv mem init  */
	lkhv_mem_init();
#endif 
	hv_event_init();
	hv_vm_init();
	ret = lkhv_parse_vmcfg((char *)VMCFG_PATH);
	if (0 > ret) {
		printf("failed to int rootfs ret %d\n", ret);
	}	

	/* vcpu percpu data init   */
	hv_vcpu_init();
	
	ret = hv_init_virq_info();
	if (0 > ret) {
		printf("failed to parse vm info\n");
	}
	
	/* TODO others  */	
}

/* hypervisor percpu init entry */
static void hv_boot_init_percpu(void)
{
	int ret = 0;

	ret = hv_vtimer_init();
	if (0 > ret)
		printf("failed to init vtimer\n");

	return;
}

LK_INIT_HOOK(hv_boot_init,
                   hv_boot_init,
                   LK_INIT_LEVEL_APPS);

LK_INIT_HOOK_FLAGS(hv_boot_init_percpu,
                   hv_boot_init_percpu,
                   LK_INIT_LEVEL_KERNEL, LK_INIT_FLAG_ALL_CPUS);
