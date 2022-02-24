/*
 * Framework for testing nested virtualization
 */

#include "svm.h"
#include "libcflat.h"
#include "processor.h"
#include "desc.h"
#include "msr.h"
#include "vm.h"
#include "smp.h"
#include "types.h"
#include "alloc_page.h"
#include "isr.h"
#include "apic.h"
#include "vmalloc.h"

/* for the nested page table*/
u64 *pte[2048];
u64 *pde[4];
u64 *pdpe;
u64 *pml4e;

struct vmcb *vmcb;

u64 *npt_get_pte(u64 address)
{
	int i1, i2;

	address >>= 12;
	i1 = (address >> 9) & 0x7ff;
	i2 = address & 0x1ff;

	return &pte[i1][i2];
}

u64 *npt_get_pde(u64 address)
{
	int i1, i2;

	address >>= 21;
	i1 = (address >> 9) & 0x3;
	i2 = address & 0x1ff;

	return &pde[i1][i2];
}

u64 *npt_get_pdpe(void)
{
	return pdpe;
}

u64 *npt_get_pml4e(void)
{
	return pml4e;
}

bool smp_supported(void)
{
	return cpu_count() > 1;
}

bool default_supported(void)
{
    return true;
}

bool vgif_supported(void)
{
	return this_cpu_has(X86_FEATURE_VGIF);
}

void default_prepare(struct svm_test *test)
{
	vmcb_ident(vmcb);
}

void default_prepare_gif_clear(struct svm_test *test)
{
}

bool default_finished(struct svm_test *test)
{
	return true; /* one vmexit */
}

bool npt_supported(void)
{
	return this_cpu_has(X86_FEATURE_NPT);
}

int get_test_stage(struct svm_test *test)
{
	barrier();
	return test->scratch;
}

void set_test_stage(struct svm_test *test, int s)
{
	barrier();
	test->scratch = s;
	barrier();
}

void inc_test_stage(struct svm_test *test)
{
	barrier();
	test->scratch++;
	barrier();
}

static void vmcb_set_seg(struct vmcb_seg *seg, u16 selector,
                         u64 base, u32 limit, u32 attr)
{
	seg->selector = selector;
	seg->attrib = attr;
	seg->limit = limit;
	seg->base = base;
}

inline void vmmcall(void)
{
	asm volatile ("vmmcall" : : : "memory");
}

static test_guest_func guest_main;

void test_set_guest(test_guest_func func)
{
	guest_main = func;
}

static void test_thunk(struct svm_test *test)
{
	guest_main(test);
	vmmcall();
}

u8 *io_bitmap;
u8 io_bitmap_area[16384];

u8 *msr_bitmap;
u8 msr_bitmap_area[MSR_BITMAP_SIZE + PAGE_SIZE];

void vmcb_ident(struct vmcb *vmcb)
{
	u64 vmcb_phys = virt_to_phys(vmcb);
	struct vmcb_save_area *save = &vmcb->save;
	struct vmcb_control_area *ctrl = &vmcb->control;
	u32 data_seg_attr = 3 | SVM_SELECTOR_S_MASK | SVM_SELECTOR_P_MASK
	    | SVM_SELECTOR_DB_MASK | SVM_SELECTOR_G_MASK;
	u32 code_seg_attr = 9 | SVM_SELECTOR_S_MASK | SVM_SELECTOR_P_MASK
	    | SVM_SELECTOR_L_MASK | SVM_SELECTOR_G_MASK;
	struct descriptor_table_ptr desc_table_ptr;

	memset(vmcb, 0, sizeof(*vmcb));
	asm volatile ("vmsave %0" : : "a"(vmcb_phys) : "memory");
	vmcb_set_seg(&save->es, read_es(), 0, -1U, data_seg_attr);
	vmcb_set_seg(&save->cs, read_cs(), 0, -1U, code_seg_attr);
	vmcb_set_seg(&save->ss, read_ss(), 0, -1U, data_seg_attr);
	vmcb_set_seg(&save->ds, read_ds(), 0, -1U, data_seg_attr);
	sgdt(&desc_table_ptr);
	vmcb_set_seg(&save->gdtr, 0, desc_table_ptr.base, desc_table_ptr.limit, 0);
	sidt(&desc_table_ptr);
	vmcb_set_seg(&save->idtr, 0, desc_table_ptr.base, desc_table_ptr.limit, 0);
	ctrl->asid = 1;
	save->cpl = 0;
	save->efer = rdmsr(MSR_EFER);
	save->cr4 = read_cr4();
	save->cr3 = read_cr3();
	save->cr0 = read_cr0();
	save->dr7 = read_dr7();
	save->dr6 = read_dr6();
	save->cr2 = read_cr2();
	save->g_pat = rdmsr(MSR_IA32_CR_PAT);
	save->dbgctl = rdmsr(MSR_IA32_DEBUGCTLMSR);
	ctrl->intercept = (1ULL << INTERCEPT_VMRUN) | (1ULL << INTERCEPT_VMMCALL);
	ctrl->iopm_base_pa = virt_to_phys(io_bitmap);
	ctrl->msrpm_base_pa = virt_to_phys(msr_bitmap);

	if (npt_supported()) {
		ctrl->nested_ctl = 1;
		ctrl->nested_cr3 = (u64)pml4e;
		ctrl->tlb_ctl = TLB_CONTROL_FLUSH_ALL_ASID;
	}
}

struct regs regs;

struct regs get_regs(void)
{
	return regs;
}

// rax handled specially below

#define SAVE_GPR_C                              \
        "xchg %%rbx, regs+0x8\n\t"              \
        "xchg %%rcx, regs+0x10\n\t"             \
        "xchg %%rdx, regs+0x18\n\t"             \
        "xchg %%rbp, regs+0x28\n\t"             \
        "xchg %%rsi, regs+0x30\n\t"             \
        "xchg %%rdi, regs+0x38\n\t"             \
        "xchg %%r8, regs+0x40\n\t"              \
        "xchg %%r9, regs+0x48\n\t"              \
        "xchg %%r10, regs+0x50\n\t"             \
        "xchg %%r11, regs+0x58\n\t"             \
        "xchg %%r12, regs+0x60\n\t"             \
        "xchg %%r13, regs+0x68\n\t"             \
        "xchg %%r14, regs+0x70\n\t"             \
        "xchg %%r15, regs+0x78\n\t"

#define LOAD_GPR_C      SAVE_GPR_C

struct svm_test *v2_test;

#define ASM_PRE_VMRUN_CMD                       \
                "vmload %%rax\n\t"              \
                "mov regs+0x80, %%r15\n\t"      \
                "mov %%r15, 0x170(%%rax)\n\t"   \
                "mov regs, %%r15\n\t"           \
                "mov %%r15, 0x1f8(%%rax)\n\t"   \
                LOAD_GPR_C                      \

#define ASM_POST_VMRUN_CMD                      \
                SAVE_GPR_C                      \
                "mov 0x170(%%rax), %%r15\n\t"   \
                "mov %%r15, regs+0x80\n\t"      \
                "mov 0x1f8(%%rax), %%r15\n\t"   \
                "mov %%r15, regs\n\t"           \
                "vmsave %%rax\n\t"              \

u64 guest_stack[10000];

int __svm_vmrun(u64 rip)
{
	vmcb->save.rip = (ulong)rip;
	vmcb->save.rsp = (ulong)(guest_stack + ARRAY_SIZE(guest_stack));
	regs.rdi = (ulong)v2_test;

	asm volatile (
		ASM_PRE_VMRUN_CMD
                "vmrun %%rax\n\t"               \
		ASM_POST_VMRUN_CMD
		:
		: "a" (virt_to_phys(vmcb))
		: "memory", "r15");

	return (vmcb->control.exit_code);
}

int svm_vmrun(void)
{
	return __svm_vmrun((u64)test_thunk);
}

extern u8 vmrun_rip;

static noinline void test_run(struct svm_test *test)
{
	u64 vmcb_phys = virt_to_phys(vmcb);

	irq_disable();
	vmcb_ident(vmcb);

	test->prepare(test);
	guest_main = test->guest_func;
	vmcb->save.rip = (ulong)test_thunk;
	vmcb->save.rsp = (ulong)(guest_stack + ARRAY_SIZE(guest_stack));
	regs.rdi = (ulong)test;
	do {
		struct svm_test *the_test = test;
		u64 the_vmcb = vmcb_phys;
		asm volatile (
			"clgi;\n\t" // semi-colon needed for LLVM compatibility
			"sti \n\t"
			"call *%c[PREPARE_GIF_CLEAR](%[test]) \n \t"
			"mov %[vmcb_phys], %%rax \n\t"
			ASM_PRE_VMRUN_CMD
			".global vmrun_rip\n\t"		\
			"vmrun_rip: vmrun %%rax\n\t"    \
			ASM_POST_VMRUN_CMD
			"cli \n\t"
			"stgi"
			: // inputs clobbered by the guest:
			"=D" (the_test),            // first argument register
			"=b" (the_vmcb)             // callee save register!
			: [test] "0" (the_test),
			[vmcb_phys] "1"(the_vmcb),
			[PREPARE_GIF_CLEAR] "i" (offsetof(struct svm_test, prepare_gif_clear))
			: "rax", "rcx", "rdx", "rsi",
			"r8", "r9", "r10", "r11" , "r12", "r13", "r14", "r15",
			"memory");
		++test->exits;
	} while (!test->finished(test));
	irq_enable();

	report(test->succeeded(test), "%s", test->name);

        if (test->on_vcpu)
	    test->on_vcpu_done = true;
}

static void set_additional_vcpu_msr(void *msr_efer)
{
	void *hsave = alloc_page();

	wrmsr(MSR_VM_HSAVE_PA, virt_to_phys(hsave));
	wrmsr(MSR_EFER, (ulong)msr_efer | EFER_SVME);
}

static void setup_svm(void)
{
	void *hsave = alloc_page();
	u64 *page, address;
	int i,j;

	wrmsr(MSR_VM_HSAVE_PA, virt_to_phys(hsave));
	wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_SVME);

	io_bitmap = (void *) ALIGN((ulong)io_bitmap_area, PAGE_SIZE);

	msr_bitmap = (void *) ALIGN((ulong)msr_bitmap_area, PAGE_SIZE);

	if (!npt_supported())
		return;

	for (i = 1; i < cpu_count(); i++)
		on_cpu(i, (void *)set_additional_vcpu_msr, (void *)rdmsr(MSR_EFER));

	printf("NPT detected - running all tests with NPT enabled\n");

	/*
	* Nested paging supported - Build a nested page table
	* Build the page-table bottom-up and map everything with 4k
	* pages to get enough granularity for the NPT unit-tests.
	*/

	address = 0;

	/* PTE level */
	for (i = 0; i < 2048; ++i) {
		page = alloc_page();

		for (j = 0; j < 512; ++j, address += 4096)
	    		page[j] = address | 0x067ULL;

		pte[i] = page;
	}

	/* PDE level */
	for (i = 0; i < 4; ++i) {
		page = alloc_page();

	for (j = 0; j < 512; ++j)
	    page[j] = (u64)pte[(i * 512) + j] | 0x027ULL;

		pde[i] = page;
	}

	/* PDPe level */
	pdpe   = alloc_page();
	for (i = 0; i < 4; ++i)
		pdpe[i] = ((u64)(pde[i])) | 0x27;

	/* PML4e level */
	pml4e    = alloc_page();
	pml4e[0] = ((u64)pdpe) | 0x27;
}

int matched;

static bool
test_wanted(const char *name, char *filters[], int filter_count)
{
        int i;
        bool positive = false;
        bool match = false;
        char clean_name[strlen(name) + 1];
        char *c;
        const char *n;

        /* Replace spaces with underscores. */
        n = name;
        c = &clean_name[0];
        do *c++ = (*n == ' ') ? '_' : *n;
        while (*n++);

        for (i = 0; i < filter_count; i++) {
                const char *filter = filters[i];

                if (filter[0] == '-') {
                        if (simple_glob(clean_name, filter + 1))
                                return false;
                } else {
                        positive = true;
                        match |= simple_glob(clean_name, filter);
                }
        }

        if (!positive || match) {
                matched++;
                return true;
        } else {
                return false;
        }
}

int main(int ac, char **av)
{
	/* Omit PT_USER_MASK to allow tested host.CR4.SMEP=1. */
	pteval_t opt_mask = 0;
	int i = 0;

	ac--;
	av++;

	__setup_vm(&opt_mask);

	if (!this_cpu_has(X86_FEATURE_SVM)) {
		printf("SVM not availble\n");
		return report_summary();
	}

	setup_svm();

	vmcb = alloc_page();

	for (; svm_tests[i].name != NULL; i++) {
		if (!test_wanted(svm_tests[i].name, av, ac))
			continue;
		if (svm_tests[i].supported && !svm_tests[i].supported())
			continue;
		if (svm_tests[i].v2 == NULL) {
			if (svm_tests[i].on_vcpu) {
				if (cpu_count() <= svm_tests[i].on_vcpu)
					continue;
				on_cpu_async(svm_tests[i].on_vcpu, (void *)test_run, &svm_tests[i]);
				while (!svm_tests[i].on_vcpu_done)
					cpu_relax();
			}
			else
				test_run(&svm_tests[i]);
		} else {
			vmcb_ident(vmcb);
			v2_test = &(svm_tests[i]);
			svm_tests[i].v2();
		}
	}

	if (!matched)
		report(matched, "command line didn't match any tests!");

	return report_summary();
}
