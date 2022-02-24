/*
 * develop by pwang for gicv3 for none secure 
 */
#include <assert.h>
#include <lk/bits.h>
#include <lk/err.h>
#include <sys/types.h>
#include <lk/debug.h>
#include <dev/interrupt/arm_gic.h>
#include <lk/reg.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <lk/init.h>
#include <platform/interrupts.h>
#include <arch/ops.h>
#include <platform/gic.h>
#include <lk/trace.h>
#include <vgic_gic.h>
#if WITH_LIB_SM
#include <lib/sm.h>
#include <lib/sm/sm_err.h>
#endif
#include <vgic_gic.h>
#include <hv_vgic.h>
#define LOCAL_TRACE 0

#if ARCH_ARM
#include <arch/arm.h>
#define iframe arm_iframe
#define IFRAME_PC(frame) ((frame)->pc)
#endif
#if ARCH_ARM64
#include <arch/arm64.h>
#define iframe arm64_iframe_short
#define IFRAME_PC(frame) ((frame)->elr)
#endif

#define GIC_MAX_PER_CPU_INT 32


/* distributor regs  */
#define GICD_CTLR               (GICD_OFFSET + 0x000)
#define GICD_TYPER              (GICD_OFFSET + 0x004)
#define GICD_IIDR               (GICD_OFFSET + 0x008)
#define GICD_TYPER2             (GICD_OFFSET + 0x00C)
#define GICD_STATUSR            (GICD_OFFSET + 0x010)
#define GICD_SETSPI_NSR         (GICD_OFFSET + 0x040)
#define GICD_CLRSPI_NSR         (GICD_OFFSET + 0x048)
#define GICD_SETSPI_SR          (GICD_OFFSET + 0x050)
#define GICD_CLRSPI_SR          (GICD_OFFSET + 0x058)
#define GICD_IGROUPR(n)         (GICD_OFFSET + 0x080 + (n) * 4)
#define GICD_ISENABLER(n)       (GICD_OFFSET + 0x100 + (n) * 4)
#define GICD_ICENABLER(n)       (GICD_OFFSET + 0x180 + (n) * 4)
#define GICD_ISPENDR(n)         (GICD_OFFSET + 0x200 + (n) * 4)
#define GICD_ICPENDR(n)         (GICD_OFFSET + 0x280 + (n) * 4)
#define GICD_ISACTIVER(n)       (GICD_OFFSET + 0x300 + (n) * 4)
#define GICD_ICACTIVER(n)       (GICD_OFFSET + 0x380 + (n) * 4)
#define GICD_IPRIORITYR(n)      (GICD_OFFSET + 0x400 + (n) * 4)
#define GICD_ITARGETSR(n)       (GICD_OFFSET + 0x800 + (n) * 4)
#define GICD_ICFGR(n)           (GICD_OFFSET + 0xc00 + (n) * 4)
#define GICD_NSACR(n)           (GICD_OFFSET + 0xe00 + (n) * 4)
#define GICD_SGIR               (GICD_OFFSET + 0xf00)
#define GICD_CPENDSGIR(n)       (GICD_OFFSET + 0xf10 + (n) * 4)
#define GICD_SPENDSGIR(n)       (GICD_OFFSET + 0xf20 + (n) * 4)
#define GICD_IGROUPR_E(n)       (GICD_OFFSET + 0x1000 + (n) * 4)
#define GICD_ISENABLER_E(n)     (GICD_OFFSET + 0x1200 + (n) * 4)
#define GICD_ICENABLER_E(n)     (GICD_OFFSET + 0x1400 + (n) * 4)
#define GICD_ISPENDR_E(n)       (GICD_OFFSET + 0x1600 + (n) * 4) 
#define GICD_ICPENDR_E(n)       (GICD_OFFSET + 0x1800 + (n) * 4)
#define GICD_ISACTIVER_E(n)     (GICD_OFFSET + 0x1a00 + (n) * 4)
#define GICD_ICACTIVER_E(n)     (GICD_OFFSET + 0x1c00 + (n) * 4)
#define GICD_IPRIORITYR_E(n)    (GICD_OFFSET + 0x2000 + (n) * 4)
#define GICD_ICFGR_E(n)         (GICD_OFFSET + 0x3000 + (n) * 4)
#define GICD_IGRPMODR_E(n)      (GICD_OFFSET + 0x3400 + (n) * 4)
#define GICD_NSACR_E(n)         (GICD_OFFSET + 0x3600 + (n) * 4)
#define GICD_IROUTER(n)         (GICD_OFFSET + 0x6100 + (n) * 8)
#define GICD_IROUTER_E(n)       (GICD_OFFSET + 0x8000 + (n) * 8)

/* redistributor regs */
#define GICR_OFFSET(cpu_id)     	(GICR_BASE + GICR_FRAME * 2 * cpu_id)
#define GICR_CTLR(cpu_id)       	(GICR_OFFSET(cpu_id) + 0x0000)
#define GICR_IIDR(cpu_id)               (GICR_OFFSET(cpu_id) + 0x0004)
#define GICR_TYPER(cpu_id)              (GICR_OFFSET(cpu_id) + 0x0008)
#define GICR_STATUSR(cpu_id)            (GICR_OFFSET(cpu_id) + 0x0010)
#define GICR_WAKER(cpu_id)              (GICR_OFFSET(cpu_id) + 0x0014)
#define GICR_MPAMIDR(cpu_id)            (GICR_OFFSET(cpu_id) + 0x0018)
#define GICR_PARTIDR(cpu_id)            (GICR_OFFSET(cpu_id) + 0x001c)
#define GICR_SETLPIR(cpu_id)            (GICR_OFFSET(cpu_id) + 0x0040)
#define GICR_CLRLPIR(cpu_id)            (GICR_OFFSET(cpu_id) + 0x0048)
#define GICR_PROPBASER(cpu_id)          (GICR_OFFSET(cpu_id) + 0x0070)
#define GICR_PENDBASER(cpu_id)          (GICR_OFFSET(cpu_id) + 0x0078)
#define GICR_INVLPIR(cpu_id)            (GICR_OFFSET(cpu_id) + 0x00a0)
#define GICR_INVALLR(cpu_id)            (GICR_OFFSET(cpu_id) + 0x00b0)
#define GICR_SYNCR(cpu_id)              (GICR_OFFSET(cpu_id) + 0x00c0)

#if 0
/* this is version 4, currently is version 3  */
#define GICR_VPROPBASER         (VLPI_OFFSET + 0x0070)
#define GICR_VPENDBASER         (VLPI_OFFSET + 0x0078)
#define GICR_VSGIR              (VLPI_OFFSET + 0x0080)
#define GICR_VSGIPENDR          (VLPI_OFFSET + 0x0088)
#endif

#define SGI_OFFSET(cpu_id)      	GICR_OFFSET(cpu_id) + GICR_FRAME
#define GICR_IGROUPR0(cpu_id)           (SGI_OFFSET(cpu_id) + 0x0080)
#define GICR_IGROUPR_E(cpu_id, n)       (SGI_OFFSET(cpu_id) + 0x0084 + (n) * 4)
#define GICR_ISENABLER0(cpu_id)         (SGI_OFFSET(cpu_id) + 0x0100)
#define GICR_ISENABLER_E(cpu_id, n)     (SGI_OFFSET(cpu_id) + 0x0104 + (n) * 4)
#define GICR_ICENABLER_E(cpu_id, n)     (SGI_OFFSET(cpu_id) + 0x0184 + (n) * 4)
#define GICR_ICENABLER0(cpu_id)         (SGI_OFFSET(cpu_id) + 0x0180)
#define GICR_ISPENDR0(cpu_id)           (SGI_OFFSET(cpu_id) + 0x0200)
#define GICR_ISPENDR_E(cpu_id, n)       (SGI_OFFSET(cpu_id) + 0x0204 + (n) * 4)
#define GICR_ICPENDR0(cpu_id)           (SGI_OFFSET(cpu_id) + 0x0280)
#define GICR_ICPENDR_E(cpu_id, n)       (SGI_OFFSET(cpu_id) + 0x0284 + (n) * 4)
#define GICR_ISACTIVER0(cpu_id)         (SGI_OFFSET(cpu_id) + 0x0300)
#define GICR_ISACTIVER_E(cpu_id, n)     (SGI_OFFSET(cpu_id) + 0x0304 + (n) * 4)
#define GICR_ICACTIVER0(cpu_id)         (SGI_OFFSET(cpu_id) + 0x0380)
#define GICR_ICACTIVER_E(cpu_id, n)     (SGI_OFFSET(cpu_id) + 0x0384 + (n) * 4)
#define GICR_IPRIORITYR(cpu_id, n)      (SGI_OFFSET(cpu_id) + 0x0400 + (n) * 4)
#define GICR_IPRIORITYR_E(cpu_id, n)    (SGI_OFFSET(cpu_id) + 0x0420 + (n) * 4)
#define GICR_ICFGR0(cpu_id)             (SGI_OFFSET(cpu_id) + 0x0c00)
#define GICR_ICFGR1(cpu_id)             (SGI_OFFSET(cpu_id) + 0x0c04)
#define GICR_ICFGR_E(cpu_id, n)         (SGI_OFFSET(cpu_id) + 0x0C08 + (n) * 4)
#define GICR_IGRPMODR0(cpu_id)          (SGI_OFFSET(cpu_id) + 0x0D00 )
#define GICR_IGRPMODR_E(cpu_id, n)      (SGI_OFFSET(cpu_id) + 0x0D04 + (n) + 4)
#define GICR_NSACR(cpu_id)              (SGI_OFFSET(cpu_id) + 0x0E00)

#define MAX_PPI_VEC             	31
#define MAX_SGI_VEC             	15
#define MAX_SPI_VEC             	1019

/* main cpu regs  */
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define GIC_REG_COUNT(bit_per_reg) DIV_ROUND_UP(MAX_INT, (bit_per_reg))
#define DEFINE_GIC_SHADOW_REG(name, bit_per_reg, init_val, init_from) \
    uint32_t (name)[GIC_REG_COUNT(bit_per_reg)] = { \
        [(init_from / bit_per_reg) ... \
         (GIC_REG_COUNT(bit_per_reg) - 1)] = (init_val) \
    }

#if WITH_LIB_SM
static DEFINE_GIC_SHADOW_REG(gicd_igroupr, 32, ~0U, 0);
#endif
static DEFINE_GIC_SHADOW_REG(gicd_itargetsr, 4, 0x01010101, 32);


#define GICR_WAKER_PS          (uint32_t) (1 << 1)   /* process sleep */
#define GICR_WAKER_CAS         (uint32_t) (1 << 2)

#define GICC_SRE_SRE           (uint64_t) (1 << 0)   /* enable system register  */
#define GICC_SRE_DFB           (uint64_t) (1 << 1)   /* disable fiq bypass  */ 
#define GICC_SRE_DIB           (uint64_t) (1 << 2)   /* disable irq bypass  */
#define GICC_SRE_ENABLE        (uint64_t) (1 << 3)   /* enable lower exception access GICC_SRE_EL2  */
#define GICC_SRE_DEFAULT       GICC_SRE_SRE|GICC_SRE_DFB|GICC_SRE_DIB|GICC_SRE_ENABLE
#define GICC_PMR_DEFAULT       (uint64_t)(0XFF)                 /* default value, the minium one  */
#define GICC_BPR_DEFAULT       0
#define GICC_IGRPEN1_ENABLE    (uint64_t)(1)

#define ICC_PMR_EL1               S3_0_C4_C6_0
#define ICC_DIR_EL1               S3_0_C12_C11_1
#define ICC_SGI1R_EL1             S3_0_C12_C11_5
#define ICC_EOIR1_EL1             S3_0_C12_C12_1
#define ICC_IAR1_EL1              S3_0_C12_C12_0
#define ICC_BPR1_EL1              S3_0_C12_C12_3
#define ICC_CTLR_EL1              S3_0_C12_C12_4
#define ICC_SRE_EL1               S3_0_C12_C12_5
#define ICC_IGRPEN1_EL1           S3_0_C12_C12_7

#define ICH_VSEIR_EL2             S3_4_C12_C9_4
#define ICC_SRE_EL2               S3_4_C12_C9_5
#define ICH_HCR_EL2               S3_4_C12_C11_0
#define ICH_VTR_EL2               S3_4_C12_C11_1
#define ICH_MISR_EL2              S3_4_C12_C11_2
#define ICH_EISR_EL2              S3_4_C12_C11_3
#define ICH_ELSR_EL2              S3_4_C12_C11_5
#define ICH_VMCR_EL2              S3_4_C12_C11_7
#define ICC_RPR_EL1               S3_0_C12_C11_3

#define ICC_CTLR_EOImode          (uint64_t)(1 << 1) 

static spin_lock_t gicd_lock;
#if WITH_LIB_SM
#define GICD_LOCK_FLAGS SPIN_LOCK_FLAG_IRQ_FIQ
#else
#define GICD_LOCK_FLAGS SPIN_LOCK_FLAG_INTERRUPTS
#endif

static uint64_t cpu_id_map[SMP_MAX_CPUS] = {0};
#define gic_lock(state)    spin_lock_save(&gicd_lock, &state, GICD_LOCK_FLAGS);
#define gic_unlock(state)  spin_unlock_restore(&gicd_lock, state, GICD_LOCK_FLAGS); 

struct int_handler_struct {
	int_handler handler;
	void *arg;
};

static struct int_handler_struct int_handler_table_per_cpu[GIC_MAX_PER_CPU_INT][SMP_MAX_CPUS];
static struct int_handler_struct int_handler_table_shared[MAX_INT-GIC_MAX_PER_CPU_INT];

uint32_t read_gicd_iidr(void)
{
	return GICREG(GICD_IIDR);
}

uint32_t read_gicr_iidr(void)
{
	return GICREG(GICR_IIDR(arch_curr_cpu_num()));
}

static struct int_handler_struct *get_int_handler(unsigned int vector, uint cpu) 
{
	struct int_handler_struct *handler = NULL;
	
	if (vector < GIC_MAX_PER_CPU_INT)
		handler =  &int_handler_table_per_cpu[vector][cpu];
	else
		handler =  &int_handler_table_shared[vector - GIC_MAX_PER_CPU_INT];
	return handler;
}

static bool arm_gic_interrupt_change_allowed(int irq) 
{
	return true;
}

void gic_set_enable(uint vector, bool enable) 
{
	int cpu_id = arch_curr_cpu_num(); 
	int reg = vector / 32;
	uint32_t mask = 1ULL << (vector % 32);
	uint64_t reg_ise = 0;
	uint64_t reg_ice = 0;

	if (vector > MAX_PPI_VEC) {
		reg_ise = GICD_ISENABLER(reg);
		reg_ice = GICD_ICENABLER(reg);
	} else {
		reg_ise = GICR_ISENABLER0(cpu_id);
		reg_ice = GICR_ICENABLER0(cpu_id);
	}

	/* written 0 has no effect */
	if (enable) {
		LTRACEF("gic set enable %u enable %d\n", vector, enable);
		LTRACEF("set 0x%llx  mask %x reg id %d\n", reg_ise, mask, reg);
		GICREG(reg_ise) = mask;
	} else  {
		GICREG(reg_ice) = mask;
		while(GICREG(GICD_CTLR) & GICD_CTRL_RWP);  
	}
}

void gic_trigger_interrupt(int vector) 
{
	int reg =  vector / 32;
    	uint32_t mask = 1ULL << (vector % 32);
	int cpu_id = arch_curr_cpu_num(); 
	uint64_t reg_isp;

	if (vector > MAX_PPI_VEC)
		reg_isp = GICD_ISPENDR(reg);
	else
		reg_isp = GICR_ISPENDR0(cpu_id);

	printf("will set interrupt %d\n", vector);
	/* written 0 has no effect  */
        GICREG(reg_isp) = mask;
}

/* clear pending  */
void gic_clear_pending(int vector)
{
	int reg =  vector >> 5;
    	uint32_t mask = 1ULL << (vector & 0x1f);
	int cpu_id = arch_curr_cpu_num(); 
	uint64_t reg_isp;

	if (vector > MAX_PPI_VEC)
		reg_isp = GICD_ICPENDR(reg);
	else
		reg_isp = GICR_ICPENDR0(cpu_id);

	//printf("will clear interrupt %d\n", vector);
	/* written 0 has no effect  */
        GICREG(reg_isp) = mask;
}

/* clear pending  */
void gic_set_isactiver(int vector)
{
	int reg =  vector / 32;
    	uint32_t mask = 1ULL << (vector % 32);
	int cpu_id = arch_curr_cpu_num(); 
	uint64_t reg_isp;

	if (vector > MAX_PPI_VEC)
		reg_isp = GICD_ISACTIVER(reg);
	else
		reg_isp = GICR_ISACTIVER0(cpu_id);

	//printf("set isactiver for %d\n", vector);
	/* written 0 has no effect  */
        GICREG(reg_isp) = mask;
}

void gic_set_icactiver(int vector)
{
	int reg =  vector >> 5;
    	uint32_t mask = 1ULL << (vector & 0x1f);
	int cpu_id = arch_curr_cpu_num(); 
	uint64_t reg_isp;

	if (vector > MAX_PPI_VEC)
		reg_isp = GICD_ICACTIVER(reg);
	else
		reg_isp = GICR_ICACTIVER0(cpu_id);

	//printf("set isactiver for %d\n", vector);
	/* written 0 has no effect  */
        GICREG(reg_isp) = mask;
}

static void gic_set_priority(uint32_t vector, 
			unsigned char priority, int cpu_id)
{
	uint64_t ipriority;

	if (vector < MAX_PPI_VEC)
		ipriority = GICR_IPRIORITYR(cpu_id, 0) + vector;
	else
		ipriority = GICD_IPRIORITYR(0) + vector;
	
	LTRACEF("gic set priority  irq %u priority %d cpu_id %d\n", 
			vector, priority, cpu_id);
	LTRACEF("ipriority %llx\n", ipriority);
	GICREG_BYTE(ipriority) = priority;
}

/* configure a cpu type 
 * @type :
 * 0 : level
 * 1 : edge */
void gic_set_cfg(uint32_t vector, int is_edge, int cpu_id)
{
	uint64_t icfgr = 0;
	uint32_t icfgr_val = 0;
	int icfg_n, icfg_off;
	spin_lock_saved_state_t state;
	
	LTRACEF("set cfg  irq %u is_edge %d cpu_id %d\n", 
			vector, is_edge, cpu_id);

	icfg_n = vector >> 4;
	icfg_off = vector & 0xf;
	//printf("icfg_off is %d vector %u is_edge %d\n", 
	//		icfg_off, vector, is_edge);

	if (vector <= MAX_PPI_VEC)
		if (icfg_n == 0)
			icfgr = GICR_ICFGR0(cpu_id);
		else
			icfgr = GICR_ICFGR1(cpu_id);	
	else
		icfgr = GICD_ICFGR(icfg_n);
	
	gic_lock(state);	
       	icfgr_val = GICREG(icfgr);
	if (is_edge)
		icfgr_val = icfgr_val | (1 << (2 * icfg_off + 1));
	else
		icfgr_val = icfgr_val & (~(1 << (2 * icfg_off + 1)));

	GICREG(icfgr) = icfgr_val;
	gic_unlock(state);
	LTRACEF("set cfg value %llx to  %x\n", icfgr, icfgr_val);
}

/* set affinity of a vector
 * @is_cluster is 1, affinity is the cluster, 
 * which is cpu_id  in  
 * must ensure vector is SPI */
void gic_set_affinity(uint32_t vector, int cpu_id, int is_cluster)
{
	/* default to all cpu cpus current cluster */
	uint64_t cpu_map = cpu_id_map[cpu_id];
	uint64_t iroute = 0;
	
	LTRACEF("set cfg  irq %u is_cluster %d cpu_id %d\n", 
			vector, is_cluster, cpu_id);
	
	if (is_cluster) 
		iroute = iroute  | 1 << 31;
	else 
		iroute = cpu_map;

	GICREG_64(GICD_IROUTER(vector)) = iroute;
	LTRACEF("set affity %llx to  %llx\n", iroute, GICD_IROUTER(vector));
}


/* init and interrupt whose  vector is @vector 
 * default group is ns-group1 
 * target : default one is to all of cpus  */
int gic_set_irq(uint32_t vector, int priority, 
		int is_edge, int enable,
		int_handler handler, void *arg)
{
	struct int_handler_struct *h;
	int cpu_id = arch_curr_cpu_num();
	spin_lock_saved_state_t state;

	if (vector >= MAX_INT)
		panic("gic_set_irq: vector out of range %d\n", vector);

	gic_set_cfg(vector, is_edge, cpu_id);	
	gic_set_priority(vector, priority, cpu_id);
	gic_set_enable(vector, 1);
       	if (vector > MAX_PPI_VEC)
		gic_set_affinity(vector, cpu_id, 0);

	spin_lock_save(&gicd_lock, &state, GICD_LOCK_FLAGS);	
	if (arm_gic_interrupt_change_allowed(vector)) {
		LTRACEF("set callback functions\n");
		h = get_int_handler(vector, cpu_id);
		h->handler = handler;
		h->arg = arg;
	}
	spin_unlock_restore(&gicd_lock, state, GICD_LOCK_FLAGS);
			

	return 0;
}

/* api : interface  */
void register_int_handler(unsigned int vector, int_handler handler, void *arg) 
{
	int priority;

	if (vector >= MAX_INT)
		panic("register_int_handler: vector out of range %d\n", vector);

	priority = GIC_PRI_IRQ;
	/* TODO IPI priority set  */
	
	gic_set_irq(vector, priority, 1, 1, handler, arg);
	return;
}

/* read ispendr for irq @vector  */
int gic_read_ispendr(unsigned int vector)
{
	int reg_id = vector >> 5;
	int reg_off = vector & 0x1f;
	unsigned int ispendr = 0;
	unsigned int cpu_id = arch_curr_cpu_num();

	if (vector > MAX_PPI_VEC)
		ispendr = GICREG(GICD_ISPENDR(reg_id));
	else
		ispendr = GICREG(GICR_ISPENDR0(cpu_id));
	return ispendr & (1 << reg_off);
}

/* init cpu map  */
static void init_cpu_map(uint32_t cpu_id)
{
	uint64_t mpidr;

	if (cpu_id >= SMP_MAX_CPUS)
		return;

	mpidr = ARM64_READ_SYSREG(MPIDR_EL1);
	mpidr = mpidr & 0xff00ffffff;
	cpu_id_map[cpu_id] = mpidr;
}

/* init percpu  */
/* about the pe init, please refer  */
static void arm_gic_init_percpu(uint32_t level) 
{
	uint32_t cpu_id = 0;
	uint32_t waker = 0;
	uint64_t val = 0;
	int i = 0;

	cpu_id = arch_curr_cpu_num();
	/* redistributor init  */
	printf("waker addr for cpu %u is %llx\n", cpu_id, GICR_WAKER(cpu_id));
	waker = GICREG(GICR_WAKER(cpu_id));
	GICREG(GICR_WAKER(cpu_id)) = (waker & ~GICR_WAKER_PS);
	
	/*  cpu interface init first  */
	val = ARM64_READ_SYSREG(ICC_SRE_EL2);
	val = val|GICC_SRE_SRE;
	ARM64_WRITE_SYSREG(ICC_SRE_EL2, val);
	ISB;
       
	/* set priority*/
	ARM64_WRITE_SYSREG(ICC_PMR_EL1, GICC_PMR_DEFAULT);
	
	/* use all of bit  */
	ARM64_WRITE_SYSREG(ICC_BPR1_EL1, GICC_BPR_DEFAULT);

	/* set EOI mode hypervisor need to set this  */
	val = ARM64_READ_SYSREG(ICC_CTLR_EL1);
	val = val | ICC_CTLR_EOImode;
	ARM64_WRITE_SYSREG(ICC_CTLR_EL1, val);

	/* disable all of ppi and sigs */
	GICREG(GICR_ICENABLER0(cpu_id)) = (uint32_t)(-1);
	GICREG(GICR_ICACTIVER0(cpu_id)) = (uint32_t)(-1);

	/* ppi or spi should be all ns-group 1  */
	GICREG(GICR_IGROUPR0(cpu_id)) = (uint32_t)(-1);

	/* SGI or PPI set interrupt */
	for (i = 0; i <= MAX_SGI_VEC; i++) {
		gic_set_cfg(i, 1, cpu_id);
		gic_set_priority(i, GIC_PRI_IPI, cpu_id);
		gic_set_enable(i, 1);
	}	

	/* Enable signaling of each interrupt group */
	ARM64_WRITE_SYSREG(ICC_IGRPEN1_EL1, GICC_IGRPEN1_ENABLE);
	init_cpu_map(cpu_id);
	ISB;

	/* vgic init  */
	vgic_gic_init();
	return;
}

LK_INIT_HOOK_FLAGS(arm_gic_init_percpu,
                   arm_gic_init_percpu,
                   LK_INIT_LEVEL_PLATFORM_EARLY, LK_INIT_FLAG_SECONDARY_CPUS);

static int arm_gic_max_cpu(void) 
{
    return (GICREG(GICD_TYPER) >> 5) & 0x7;
}

static uint32_t get_max_ints(void)
{
	int max_ints;
	uint32_t typer;
	
	typer = GICREG(GICD_TYPER);
	max_ints = (typer & 0x1f);
	//max_ints = 32 * (max_ints + 1) -1;
	return max_ints;
}

uint32_t get_max_spi(void)
{
	uint32_t max_ints;

	max_ints = get_max_ints();
	max_ints = 32 * (max_ints + 1) -1;
	return max_ints;
}

/* gic init */
void arm_gic_init(void) 
{
	uint32_t i;
	uint32_t max_ints;
	uint32_t reg_num;

	max_ints = get_max_ints();
	reg_num = max_ints + 1; 

	/* disable all of shared interrupt */
	for (i = 0; i < reg_num; i++)
		GICREG(GICD_ICENABLER(i)) = (uint32_t)(-1);	
	while(GICREG(GICD_CTLR) & GICD_CTRL_RWP) ;  

	/* SPI should be all none-secure group1  */
	for (i = 0; i < reg_num; i++)
		GICREG(GICD_IGROUPR(i)) = (uint32_t)(-1);

	/* SPI should target to this primary cpu ?????  */
	GICREG(GICD_CTLR) = (GICD_CTRL_ARE_NS|GICD_CTRL_EnableGrp1NS);
	while(GICREG(GICD_CTLR) & GICD_CTRL_RWP) ;  
	arm_gic_init_percpu(0);

	return;
}

#define MPIDR_AFF0_MASK   ((uint64_t)(0xff))

/* each cluster should less than 16 cpus */
uint16_t calc_tglist(uint32_t *cpu_mask, uint64_t cluster)
{
	uint32_t i = 0;
	uint16_t tglist = 0;
	uint64_t cluster_id;
	uint32_t cpu;

	for (i = 0; i <  sizeof(*cpu_mask); i++) {
		if (!(*cpu_mask & (1 << i)))
			continue;

		cluster_id = cpu_id_map[i] & (~MPIDR_AFF0_MASK);
		cpu = cpu_id_map[i] & MPIDR_AFF0_MASK;
		if (cluster_id != cluster)
		       continue;
		
		tglist = tglist | (1 << cpu);
		*cpu_mask = *cpu_mask & (~(1 << i));
	}
	return tglist;
}

/* api interface : send sgi 
 * maybe  does not need flags, only send ns-group1 sgi,
 * so the max cpu is 32 because of cpu_mask size */
/* NSATT  */
status_t arm_gic_sgi(u_int irq, u_int flags, u_int cpu_mask) 
{
	/* ICC_SGI1R_EL1  */
	uint32_t i = 0;
	uint64_t cluster;
	uint16_t tglist = 0;
	uint64_t val;

	for (i = 0; i < sizeof(cpu_mask); i++) {
		if (!(cpu_mask & (1 << i)))
			continue;
		
		cluster = cpu_id_map[i] & (~MPIDR_AFF0_MASK);
		tglist = calc_tglist(&cpu_mask, cluster);
		val = (cluster & 0xff00000000) << (48 - 32)|
		      (cluster & 0x0000ff0000) << (32 - 16)|
		      (cluster & 0x000000ff00) << (16 - 8)|
		      irq << 24|tglist;

		ARM64_WRITE_SYSREG(ICC_SGI1R_EL1, val); 	
	}

	return NO_ERROR;
}

status_t mask_interrupt(unsigned int vector) 
{
	if (vector >= MAX_INT)
		return ERR_INVALID_ARGS;
	
	if (arm_gic_interrupt_change_allowed(vector))
		gic_set_enable(vector, false);

	return NO_ERROR;
}

status_t unmask_interrupt(unsigned int vector) 
{
	if (vector >= MAX_INT)
		return ERR_INVALID_ARGS;
	
	if (arm_gic_interrupt_change_allowed(vector))
		gic_set_enable(vector, true);
	
	return NO_ERROR;
}

static enum handler_return __platform_irq(struct iframe *frame) 
{
	// get the current vector
	uint32_t iar = ARM64_READ_SYSREG(ICC_IAR1_EL1);
	unsigned int vector = iar & 0x3ff;
	uint32_t cpu = arch_curr_cpu_num();
	struct int_handler_struct *handler = NULL;	
	
	if (vector >= 0x3fe) {
		// spurious
		return INT_NO_RESCHEDULE;
	}
	LTRACEF("platform irq\n");
	
	THREAD_STATS_INC(interrupts);
	KEVLOG_IRQ_ENTER(vector);
	
	cpu = arch_curr_cpu_num();
	LTRACEF_LEVEL(2, "iar 0x%x cpu %u currthread %p vector %d pc 0x%lx\n", iar, cpu,
                  get_current_thread(), vector, (uintptr_t)IFRAME_PC(frame));

	// deliver the interrupt
	enum handler_return ret;

	ret = INT_NO_RESCHEDULE;
	handler = get_int_handler(vector, cpu);
	if (handler->handler) {
		void *arg = handler->arg;

		/* virtual interrupt routing needs irq as interrupt */
		if (handler->arg == (void *)INT_IRQ)
			arg = (void *)(long)vector;

		ret = handler->handler(arg);
	}

	/*  2 steps interrupt deactive   */
	ARM64_WRITE_SYSREG(ICC_EOIR1_EL1, iar); /* first lower the priority */
	
	/* hypervisor interrupt deactive them here, guest irq will be deactive by eoi from guest  */
	if (handler->arg != (void *)INT_IRQ)
		ARM64_WRITE_SYSREG(ICC_DIR_EL1, iar);

	LTRACEF_LEVEL(2, "cpu %u exit %d\n", cpu, ret);
	KEVLOG_IRQ_EXIT(vector);
	return ret;
}

enum handler_return platform_irq(struct iframe *frame) 
{
    hv_virq_from_lrs();
    return __platform_irq(frame);
}

void platform_fiq(struct iframe *frame) 
{
    PANIC_UNIMPLEMENTED;
}

/* interrupt debug related code */
#include <lib/console.h>
int inter_enable(int argc, const console_cmd_args *argv)
{
	int vector =  argv[1].i;
	int enable =  argv[2].i;
	printf("vector %d %d\n", vector, enable);
	gic_set_enable(vector, enable);
	return 0;
}

int inter_triger(int argc, const console_cmd_args *argv)
{
	int vector =  argv[1].i;
	
	printf("enable vector %d\n", vector);
	gic_set_enable(vector, 1);
	printf("triger vector %d\n", vector);
	gic_trigger_interrupt(vector);
	return 0;
}

/* interrupts tests  */
#define GIC_TEST_IRQ 100
enum handler_return test_irq_handler(void *arg)
{
	printf("<<<<<<<<<<<<<<<<test irq is coming>>>>>>>>>>>>>>>\n");
	return 0;
}

enum handler_return test_sig_handler(void *arg)
{
	printf("<<<<<<<<<<<<<<<<test sgi is coming>>>>>>>>>>>>>>>>>\n");
	return 0;
}

/* dump the status   */
void gic_dump_status(uint32_t vector, int cpu_id)
{
	int reg_id;
	int reg_id1;

	printf("information related to %u cpu _id %d:\n", vector, cpu_id);
	printf("GICD_CTLR   : 0x%x\n", GICREG(GICD_CTLR));
	printf("GICR_WAKER  : 0x%x\n", GICREG(GICR_WAKER(cpu_id)));	
	printf("ICC_SRE_EL2 : 0x%llx\n", ARM64_READ_SYSREG(ICC_SRE_EL2));
	printf("ICC_PMR_EL1 : 0x%llx\n", ARM64_READ_SYSREG(ICC_PMR_EL1)); 
	printf("ICC_BPR1_EL1 : 0x%llx\n", ARM64_READ_SYSREG(ICC_BPR1_EL1));
	printf("ICC_RPR1_EL1: %llx\n", ARM64_READ_SYSREG(ICC_RPR_EL1));	
	printf("ICC_IGRPEN1_EL1: %llx\n", ARM64_READ_SYSREG(ICC_IGRPEN1_EL1));

	reg_id = vector >> 5;
	reg_id1 = vector >> 4;
	if (vector > MAX_PPI_VEC) {
		printf("GICD_ISENABLER(%d): %llx value %x\n", reg_id, GICD_ISENABLER(reg_id),  GICREG(GICD_ISENABLER(reg_id)));
		printf("GICD_ICENABLER(%d): %llx value %x\n", reg_id, GICD_ICENABLER(reg_id),  GICREG(GICD_ICENABLER(reg_id)));
		printf("GICD_IPRIORITYR: %llx value %x\n", GICD_IPRIORITYR(0) + vector, 
					GICREG_BYTE(GICD_IPRIORITYR(0) + vector));
		printf("GICD_IROUTE(%d): %llx\n",  vector, GICREG_64(GICD_IROUTER(vector)));	
		printf("GICD_IGROUPR(%d): %x\n", reg_id, GICREG(GICD_IGROUPR(reg_id)));	
		printf("GICD_ISACTIVER(%d): %x\n", reg_id, GICREG(GICD_ISACTIVER(reg_id)));
		printf("GICD_ISPENDR(%d): %x\n", reg_id, GICREG(GICD_ISPENDR(reg_id)));
		printf("GICD_ICFGR(%d): %x\n", reg_id1, GICREG(GICD_ICFGR(reg_id1)));
	} else {
		printf("GICR_ISENABLER0: %llx value %x\n", GICR_ISENABLER0(cpu_id),  GICREG(GICR_ISENABLER0(cpu_id)));
		printf("GICR_ICENABLER0: %llx value %x\n", GICR_ICENABLER0(cpu_id),  GICREG(GICR_ICENABLER0(cpu_id)));
		printf("GICR_IPRIORITYR: %llx value %x\n", GICR_IPRIORITYR(cpu_id, 0) + vector, 
								GICREG_BYTE(GICR_IPRIORITYR(cpu_id, 0) + vector));
		printf("GICR_IGROUPR0: %x\n", GICREG(GICR_IGROUPR0(cpu_id)));	
		printf("GICR_ISACTIVER0: %x\n", GICREG(GICR_ISACTIVER0(cpu_id)));
		printf("GICR_ISPENDR0: %x\n", GICREG(GICR_ISPENDR0(cpu_id)));
		printf("GICR_ICFGR(%d): %x\n", 0, GICREG(GICR_ICFGR0(cpu_id)));
		printf("GICR_ICFGR(%d): %x\n", 1, GICREG(GICR_ICFGR1(cpu_id)));
	}
	//printf("iar is %llx\n", ARM64_READ_SYSREG(ICC_IAR1_EL1));
}

void gic_ints_test(void)
{
	printf("-----------------------------gic ints test start--------------------------\n");
	register_int_handler(GIC_TEST_IRQ, test_irq_handler, NULL);
	gic_trigger_interrupt(GIC_TEST_IRQ);
	gic_dump_status(GIC_TEST_IRQ, arch_curr_cpu_num());
	printf("-----------------------------gic ints test end---------------------------\n");
		
}
//LK_INIT_HOOK(gic_ints_test, &gic_ints_test, LK_INIT_LEVEL_KERNEL);

int gic_irq_test(int argc, const console_cmd_args *argv)
{
	int irq;

	if (argc < 2) {
		printf("lacks of args\n");
		return -1;
	}
	irq = argv[1].i;

	printf("-----------------------------gic ints test start--------------------------\n");
	printf("irq is %d\n", irq);
	register_int_handler(irq, test_sig_handler, NULL);
	if (irq < MAX_SGI_VEC)
		arm_gic_sgi(irq, 0, 1); 
	else
		gic_trigger_interrupt(irq);
	gic_dump_status(irq, arch_curr_cpu_num());
	printf("-----------------------------gic ints test end---------------------------\n");
	return 0;
}

int gic_test_sgi(int argc, const console_cmd_args *argv)
{
	int irq, cpu;
	spin_lock_saved_state_t state;
	struct int_handler_struct *h;

	if (argc < 3) {
		printf("lacks of args\n");
		return -1;
	}

	irq = argv[1].i;
	cpu = argv[2].i;

	printf("-------------------gic sgi test start cpu %d irq %d-------------\n", cpu, irq);
	spin_lock_save(&gicd_lock, &state, GICD_LOCK_FLAGS);	
	h = get_int_handler(irq, cpu);
	h->handler = test_sig_handler;
	h->arg = NULL;
	spin_unlock_restore(&gicd_lock, state, GICD_LOCK_FLAGS);
	arm_gic_sgi(irq, 0, 1 << cpu); 
	printf("-----------------------------gic sgi test end---------------------------\n");
	return 0;
}

int gic_dump_lr(int argc, const console_cmd_args *argv)
{
	int lr;

	if (argc < 2) {
		printf("lacks of args\n");
		return -1;
	}

	lr = argv[1].i;

	printf("------------------dump lr %d-------------\n", lr);
	printf("lr value is %llx\n", vgic_read_lr_reg(0));
	return 0;
}

int gic_dump_irq(int argc, const console_cmd_args *argv)
{
	int irq;

	if (argc < 2) {
		printf("lacks of args\n");
		return -1;
	}

	irq = argv[1].i;

	printf("------------------dump irq %d status-------------\n", irq);
	gic_dump_status(irq, 0);	
	return 0;
}

int gic_clear_irq(int argc, const console_cmd_args *argv)
{
	int irq;

	if (argc < 2) {
		printf("lacks of args\n");
		return -1;
	}

	irq = argv[1].i;

	printf("------------------dump irq %d status-------------\n", irq);
	gic_clear_pending(irq);	
	return 0;
}

int gic_active_irq(int argc, const console_cmd_args *argv)
{
	int irq;

	if (argc < 2) {
		printf("lacks of args\n");
		return -1;
	}

	irq = argv[1].i;

	printf("------------------set active irq %d-------------\n", irq);
	gic_set_isactiver(irq);
	return 0;	
}

int gic_deactive_irq(int argc, const console_cmd_args *argv)
{
	int irq;

	if (argc < 2) {
		printf("lacks of args\n");
		return -1;
	}

	irq = argv[1].i;

	printf("------------------deactive irq %d -------------\n", irq);
	gic_set_icactiver(irq);	
	return 0;
}

int gic_level_test(int argc, const console_cmd_args *argv)
{
	int irq;

	if (argc < 2) {
		printf("lacks of args\n");
		return -1;
	}

	irq = argv[1].i;
	gic_set_irq(irq, 0xa0, 0, 1, test_irq_handler, NULL);
	return -1;
}

STATIC_COMMAND_START
STATIC_COMMAND("enable", "enable/disable interrupt", &inter_enable)
STATIC_COMMAND("triger", "set interrupt pending", &inter_triger)
STATIC_COMMAND("testirq", "test irq sending", &gic_irq_test)
STATIC_COMMAND("testsgi", "test sgi sending", &gic_test_sgi)
STATIC_COMMAND("dumplr", "dump lr value", &gic_dump_lr)
STATIC_COMMAND("dumpirq", "dump irq value", &gic_dump_irq)
STATIC_COMMAND("cpend", "clear irq pending", &gic_clear_irq)
STATIC_COMMAND("active", "active irq", &gic_active_irq)
STATIC_COMMAND("deactive", "deactive irq", &gic_deactive_irq)
STATIC_COMMAND("testlirq", "test level irq ", &gic_level_test)
STATIC_COMMAND_END(gic);
