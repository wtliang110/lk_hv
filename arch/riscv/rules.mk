LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += $(LOCAL_DIR)/start.S
MODULE_SRCS += $(LOCAL_DIR)/arch.c
MODULE_SRCS += $(LOCAL_DIR)/asm.S
MODULE_SRCS += $(LOCAL_DIR)/exceptions.c
MODULE_SRCS += $(LOCAL_DIR)/thread.c
MODULE_SRCS += $(LOCAL_DIR)/mmu.cpp
MODULE_SRCS += $(LOCAL_DIR)/mp.c
MODULE_SRCS += $(LOCAL_DIR)/sbi.c
MODULE_SRCS += $(LOCAL_DIR)/spinlock.c
MODULE_SRCS += $(LOCAL_DIR)/time.c

# one file uses slightly complicated designated initializer
MODULE_CFLAGS += -Wno-override-init

SMP_MAX_CPUS ?= 1
RISCV_MMU ?= none
RISCV_FPU ?= false
SUBARCH ?= 32
RISCV_MODE ?= machine

GLOBAL_DEFINES += SMP_MAX_CPUS=$(SMP_MAX_CPUS)
GLOBAL_DEFINES += PLATFORM_HAS_DYNAMIC_TIMER=1

ifeq (true,$(call TOBOOL,$(WITH_SMP)))
GLOBAL_DEFINES += WITH_SMP=1
endif

ifeq (true,$(call TOBOOL,$(RISCV_FPU)))
GLOBAL_DEFINES += RISCV_FPU=1
endif

ifeq ($(strip $(RISCV_MODE)),machine)
$(info RISCV: Machine Mode)
GLOBAL_DEFINES += RISCV_M_MODE=1
ifneq ($(RISCV_MMU),none)
$(error RISCV mmu not supported in machine mode)
endif
else ifeq ($(strip $(RISCV_MODE)),supervisor)
$(info RISCV: Supervisor Mode)
GLOBAL_DEFINES += RISCV_S_MODE=1
else
$(error Unknown RISC-V mode: "$(strip $(RISCV_MODE))" (valid values are "machine", "supervisor"))
endif

ifeq ($(RISCV_MMU),sv48)
ifeq ($(SUBARCH),32)
$(error RISCV: sv48 mmu not supported for 32 bit riscv)
endif
$(info RISCV: MMU sv48)
GLOBAL_DEFINES += RISCV_MMU=48
WITH_KERNEL_VM ?= 1

# 48 bits split between two 47 bit halves
KERNEL_ASPACE_BASE := 0xffff800000000000
KERNEL_ASPACE_SIZE := 0x0000800000000000
USER_ASPACE_BASE   := 0x0000000001000000
USER_ASPACE_SIZE   := 0x00007ffffe000000

else ifeq ($(RISCV_MMU),sv39)
ifeq ($(SUBARCH),32)
$(error RISCV: sv39 mmu not supported for 32 bit riscv)
endif
$(info RISCV: MMU sv39)
GLOBAL_DEFINES += RISCV_MMU=39
WITH_KERNEL_VM ?= 1

# 39 bits split between two 38 bit halves
KERNEL_ASPACE_BASE := 0xffffffc000000000
KERNEL_ASPACE_SIZE := 0x0000004000000000
USER_ASPACE_BASE   := 0x0000000001000000
USER_ASPACE_SIZE   := 0x0000003ffe000000

else ifeq ($(RISCV_MMU),sv32)
$(info RISCV: MMU sv32)
GLOBAL_DEFINES += RISCV_MMU=32
WITH_KERNEL_VM ?= 1

# 32 bits split between two 31 bit halves
KERNEL_ASPACE_BASE := 0x80000000
KERNEL_ASPACE_SIZE := 0x80000000
USER_ASPACE_BASE   := 0x01000000
USER_ASPACE_SIZE   := 0x7e000000

else ifeq ($(RISCV_MMU),none)
else
$(error Unknown RISCV_MMU: "$(strip $(RISCV_MMU))" (valid values are "none", "sv32", "sv39", "sv48"))
endif

ifeq (true,$(call TOBOOL,$(WITH_KERNEL_VM)))

GLOBAL_DEFINES += \
    ARCH_HAS_MMU=1 \
    KERNEL_ASPACE_BASE=$(KERNEL_ASPACE_BASE) \
    KERNEL_ASPACE_SIZE=$(KERNEL_ASPACE_SIZE) \
    USER_ASPACE_BASE=$(USER_ASPACE_BASE) \
    USER_ASPACE_SIZE=$(USER_ASPACE_SIZE)

KERNEL_BASE ?= $(KERNEL_ASPACE_BASE)+$(MEMBASE)
KERNEL_LOAD_OFFSET ?= 0

GLOBAL_DEFINES += KERNEL_BASE=$(KERNEL_BASE)
GLOBAL_DEFINES += KERNEL_LOAD_OFFSET=$(KERNEL_LOAD_OFFSET)

else # no kernel vm

KERNEL_BASE ?= $(MEMBASE)
KERNEL_LOAD_OFFSET ?= 0

endif

ROMBASE ?= 0

GLOBAL_DEFINES += ROMBASE=$(ROMBASE)
GLOBAL_DEFINES += MEMBASE=$(MEMBASE)
GLOBAL_DEFINES += MEMSIZE=$(MEMSIZE)

# if ARCH_riscv{32|64}_TOOLCHAIN_PREFIX is set use it as an override
# for toolchain prefix.
ifdef ARCH_$(ARCH)$(SUBARCH)_TOOLCHAIN_PREFIX
TOOLCHAIN_PREFIX := $(ARCH_$(ARCH)$(SUBARCH)_TOOLCHAIN_PREFIX)
endif

# default toolchain is riscv{32|64}-elf-. assume its in the path.
ifndef TOOLCHAIN_PREFIX
TOOLCHAIN_PREFIX := riscv$(SUBARCH)-elf-
endif

# based on 32 or 64 bitness, select the right toolchain and some
# compiler codegen flags
ifeq ($(SUBARCH),32)
ARCH_COMPILEFLAGS := -march=rv32imac -mabi=ilp32
# override machine for ld -r
GLOBAL_MODULE_LDFLAGS += -m elf32lriscv
else ifeq ($(SUBARCH),64)
GLOBAL_DEFINES += IS_64BIT=1
ARCH_COMPILEFLAGS := -march=rv64imac -mabi=lp64 -mcmodel=medany
# override machine for ld -r
GLOBAL_MODULE_LDFLAGS += -m elf64lriscv
else
$(error SUBARCH not set or set to something unknown)
endif

# embedded switch sets the default compile optimization and passes
# a flag to the code to switch other things on
ifeq (true,$(call TOBOOL,$(ARCH_RISCV_EMBEDDED)))
ARCH_OPTFLAGS ?= -Os
GLOBAL_DEFINES += ARCH_RISCV_EMBEDDED=1
WITH_LINKER_GC ?= 1
else
ARCH_OPTFLAGS ?= -O2
WITH_LINKER_GC ?= 0
endif

LIBGCC := $(shell $(TOOLCHAIN_PREFIX)gcc $(GLOBAL_COMPILEFLAGS) $(ARCH_COMPILEFLAGS) $(GLOBAL_CFLAGS) -print-libgcc-file-name)
$(info LIBGCC = $(LIBGCC))

# potentially generated files that should be cleaned out with clean make rule
GENERATED += \
	$(BUILDDIR)/linker-onesegment.ld \
	$(BUILDDIR)/linker-twosegment.ld

# rules for generating the linker script
$(BUILDDIR)/linker-%.ld: $(LOCAL_DIR)/linker-%.ld $(wildcard arch/*.ld) linkerscript.phony
	@echo generating $@
	@$(MKDIR)
	$(NOECHO)sed "s/%BITS%/$(SUBARCH)/g;s/%ROMBASE%/$(ROMBASE)/;s/%MEMBASE%/$(MEMBASE)/;s/%MEMSIZE%/$(MEMSIZE)/;s/%KERNEL_BASE%/$(KERNEL_BASE)/;s/%KERNEL_LOAD_OFFSET%/$(KERNEL_LOAD_OFFSET)/;s/%VECTOR_BASE_PHYS%/$(VECTOR_BASE_PHYS)/" < $< > $@.tmp
	@$(call TESTANDREPLACEFILE,$@.tmp,$@)

linkerscript.phony:
.PHONY: linkerscript.phony

# select the appropriate linker script based on if we're a one or two segment system
ifeq (true,$(call TOBOOL,$(ARCH_RISCV_TWOSEGMENT)))
GLOBAL_DEFINES += ARCH_RISCV_TWOSEGMENT=1
LINKER_SCRIPT += $(BUILDDIR)/linker-twosegment.ld
# set MAXPAGESIZE to 8 to cause the linker script to pack things in much tighter than
# a paged sytem would.
# NOTE: 8 seems to be about as far as you can go. experienced some extra stuffed words
# when using 4.
ARCH_LDFLAGS += -z max-page-size=8
else
GLOBAL_DEFINES += ARCH_RISCV_TWOSEGMENT=0
LINKER_SCRIPT += $(BUILDDIR)/linker-onesegment.ld
endif

include make/module.mk
