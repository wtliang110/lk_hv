#ifndef _X86_FWCFG_H_
#define _X86_FWCFG_H_

#include <stdint.h>
#include <stdbool.h>

#define FW_CFG_SIGNATURE        0x00
#define FW_CFG_ID               0x01
#define FW_CFG_UUID             0x02
#define FW_CFG_RAM_SIZE         0x03
#define FW_CFG_NOGRAPHIC        0x04
#define FW_CFG_NB_CPUS          0x05
#define FW_CFG_MACHINE_ID       0x06
#define FW_CFG_KERNEL_ADDR      0x07
#define FW_CFG_KERNEL_SIZE      0x08
#define FW_CFG_KERNEL_CMDLINE   0x09
#define FW_CFG_INITRD_ADDR      0x0a
#define FW_CFG_INITRD_SIZE      0x0b
#define FW_CFG_BOOT_DEVICE      0x0c
#define FW_CFG_NUMA             0x0d
#define FW_CFG_BOOT_MENU        0x0e
#define FW_CFG_MAX_CPUS         0x0f

/* Dummy entries used when running on bare metal */
#define FW_CFG_MAX_RAM		0x11

#define FW_CFG_NUM_ENTRIES      (FW_CFG_MAX_RAM + 1)

#define FW_CFG_WRITE_CHANNEL    0x4000
#define FW_CFG_ARCH_LOCAL       0x8000
#define FW_CFG_ENTRY_MASK       ~(FW_CFG_WRITE_CHANNEL | FW_CFG_ARCH_LOCAL)

#define FW_CFG_INVALID          0xffff

#define BIOS_CFG_IOPORT 0x510

#define FW_CFG_ACPI_TABLES (FW_CFG_ARCH_LOCAL + 0)
#define FW_CFG_SMBIOS_ENTRIES (FW_CFG_ARCH_LOCAL + 1)
#define FW_CFG_IRQ0_OVERRIDE (FW_CFG_ARCH_LOCAL + 2)

extern bool no_test_device;

static inline bool test_device_enabled(void)
{
	return !no_test_device;
}

uint8_t fwcfg_get_u8(unsigned index);
uint16_t fwcfg_get_u16(unsigned index);
uint32_t fwcfg_get_u32(unsigned index);
uint64_t fwcfg_get_u64(unsigned index);

unsigned fwcfg_get_nb_cpus(void);

#endif

