/* mmio functions
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

#include <stdint.h>
#include <lk/trace.h>
#include <lk/err.h>
#include <string.h>
#include <rbtree.h>
#include <arch/spinlock.h>
#include <hv_mmio.h>
#include <hv_vcpu.h>
#include <hv_vm.h>

#define LOCAL_TRACE 0
/* mmio space */
struct mmio_item {
	paddr_t mmio_start;
	uint64_t mmio_len;
	struct mmio_ops *mops;
	void *arg;
	struct rb_node rb_node;
};

/* init mmio space of a vm   */
void hv_vm_mmio_init(struct lk_vm *vm)
{
	struct mmio_space *mmio = &(vm->mmio);
	
	mmio->mmio_root = RB_ROOT;
	spin_lock_init(&mmio->lock);
}

/* register mmio address space  */
int hv_mmio_register(struct lk_vm *vm, paddr_t addr, uint64_t len,
	       			struct mmio_ops *mops, void *args)
{
	paddr_t end;
	struct mmio_item *mmio = NULL;
	struct mmio_item *mmio_new = NULL;
	struct mmio_space *m_space = NULL;
	struct rb_node **link = NULL;
	struct rb_node *parent = NULL;
        spin_lock_saved_state_t state;
	
	mmio_new = malloc(sizeof(*mmio_new));
	if (NULL == mmio_new)
		return ERR_NO_MEMORY;
	memset(mmio_new, 0, sizeof(*mmio_new));
	
	mmio_new->mmio_start = addr;
	mmio_new->mmio_len = len;
	mmio_new->mops = mops;
	mmio_new->arg = args;

	m_space = &(vm->mmio);
	link = &m_space->mmio_root.rb_node;
	
	end = addr + len;
        spin_lock_irqsave(&m_space->lock, state);
	while (*link) {
		paddr_t mmio_start, mmio_end;

                parent = *link;
		mmio = containerof(parent, struct mmio_item, rb_node);
		mmio_start = mmio->mmio_start;
		mmio_end = mmio_start + mmio->mmio_len;
               			
                if (end <= mmio_start)
                        link = &parent->rb_left;
                else if (addr >= mmio_end)
                        link = &parent->rb_right;
                else if (addr >= mmio_start && end <= mmio_end)
                        goto failed;
		else
			goto failed;
        }

        rb_link_node(&(mmio_new->rb_node), parent, link);
        rb_insert_color(&(mmio_new->rb_node), &(m_space->mmio_root));
	spin_unlock_irqrestore(&m_space->lock, state);
	LTRACEF("register mmio space for addr %lx len %llu\n", addr, len);
	printf("register mmio space for addr %lx len %llx\n", addr, len);

	return 0;

failed:
	spin_unlock_irqrestore(&m_space->lock, state);
	free(mmio_new);
	return ERR_ALREADY_EXISTS;
}

/* mmio space find  */
static struct mmio_item * mmio_space_find(struct mmio_space *m_space, paddr_t addr, int len)
{
	paddr_t end;
	struct rb_node **link = &m_space->mmio_root.rb_node;
	struct rb_node *parent = NULL;
	struct mmio_item *mmio = NULL;
	spin_lock_saved_state_t state;

	LTRACEF("find mmio space for addr %lx\n", addr);
	end = addr + len;
	spin_lock_irqsave(&m_space->lock, state);
	while (*link) {
		paddr_t mmio_start, mmio_end;

                parent = *link;
		mmio = containerof(parent, struct mmio_item, rb_node);
		mmio_start = mmio->mmio_start;
		mmio_end = mmio_start + mmio->mmio_len;
		LTRACEF("mmio space  (%lx:%lx)\n", mmio_start, mmio_end);
               			
                if (end <= mmio_start) {
                        link = &parent->rb_left;
			LTRACEF("go to left\n");
		} else if (addr >= mmio_end) {
                        link = &parent->rb_right;
			LTRACEF("go to right\n");
		} else if (addr >= mmio_start && end <= mmio_end) {
			LTRACEF("hit\n");
			spin_unlock_irqrestore(&m_space->lock, state);
                        return mmio;
		} else {
			LTRACEF("overlap\n");
			break;
		}
        }
	spin_unlock_irqrestore(&m_space->lock, state);

        return NULL;
}

/* mmio handler called by data abort exceptions */
int hv_mmio_handler(paddr_t addr, int len, int is_write, uint64_t data, int is_signed)
{
	int ret = 0;
	struct lk_vcpu *vcpu;
	struct mmio_item *mmio;
	struct lk_vm *vm = NULL;

	/* find the mmio which addr-len in  */
	vcpu = current_vcpu();
	vm = vcpu->vm;	
	mmio = mmio_space_find(&(vm->mmio), addr, len);
	if (mmio == NULL) {
		ret = 0;
		LTRACEF("failed to find mmio space for addr %lx\n", addr);
		goto out;
	}

	if (is_write) {
		LTRACEF("write %llx to addr %lx len %d\n", data, addr, len);
		ret = mmio->mops->write(vcpu, addr, len, data, mmio->arg);
	} else {
		uint64_t ret_data;
		uint64_t *dest = (uint64_t *)data;
		int size_bit = len * 8;

		LTRACEF("read from  addr %lx len %d\n", addr, len);
		ret = mmio->mops->read(vcpu, addr, len, &ret_data, mmio->arg);
		if (is_signed && ((1 << (size_bit - 1)) & (ret_data)))
			ret_data |= (uint64_t)(~0) << size_bit;

		*dest = ret_data;	
	}
out:	
	return ret;
}	
