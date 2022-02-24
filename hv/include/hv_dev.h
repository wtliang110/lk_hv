#ifndef __VM_DEV_H__
#define __VM_DEV_H__
#include <hv_mem.h>

struct vm_dev {
	int type;
	uint32_t pirq;
	uint32_t virq;
	struct dev_mem_seg *mem_seg;
};

#endif 
