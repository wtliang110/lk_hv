LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_DEPS := \
	lib/libc \
	lib/debug \
	lib/heap \
	lib/fs \
	lib/fdt \
	lib/elf 

MODULE_SRCS := \
	$(LOCAL_DIR)/hv_boot.c \
	$(LOCAL_DIR)/vm_config.c \
	$(LOCAL_DIR)/hv_mem.c \
	$(LOCAL_DIR)/hv_vm.c \
	$(LOCAL_DIR)/test/utest.c \
	$(LOCAL_DIR)/hv_image.c \
	$(LOCAL_DIR)/hv_vcpu.c \
	$(LOCAL_DIR)/hv_event.c \
	$(LOCAL_DIR)/hv_irq.c \
	$(LOCAL_DIR)/hv_mmio.c


MODULE_DEPS += hv/arch/armv8a \
	       hv/utils

include make/module.mk
