LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_DEPS := \
        lib/libc \
        lib/debug \
        lib/heap \
        lib/fs \
        lib/fdt

MODULE_SRCS := \
        $(LOCAL_DIR)/hv_arch_mem.c\
	$(LOCAL_DIR)/hv_arch_vcpu.c\
	$(LOCAL_DIR)/hv_arch_vcpu_asm.S\
	$(LOCAL_DIR)/hv_psci_emulation.c \
	$(LOCAL_DIR)/hv_vgic.c \
	$(LOCAL_DIR)/vgic_gic.c \
	$(LOCAL_DIR)/hv_arch_traps.c\
	$(LOCAL_DIR)/hv_arch_cfg.c \
	$(LOCAL_DIR)/vgic_mmio.c\
	$(LOCAL_DIR)/hv_vtimer.c

include make/module.mk
