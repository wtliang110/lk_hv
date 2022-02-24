#ifndef __HV_MEM_H__
#define __HV_MEM_H__

#include <hv_arch_mem.h>
#include <arch/spinlock.h>


/* memory information */
struct phy_mem_seg {
	paddr_t pa;
	paddr_t ipa;
	struct list_node pgs;
};

/* dev memory segment  */
struct dev_mem_seg {
	paddr_t pa;
	paddr_t ipa;
};

struct vm_mem {
	spin_lock_t mem_lock;            /* lock to protect memory */
	struct arch_vm_mem arch_mem;     /* memory info related to arch */
	int seg_num;                     /* physical memory seg number  */
	struct phy_mem_seg *mem_seg;     /* physical memory allocated for thsi vm */
	int need_flush;
};

void lkhv_unmap_ram(struct vm_mem *mem);
#endif 
