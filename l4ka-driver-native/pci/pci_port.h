#ifndef __PCI__PCI_PORT_H__
#define __PCI__PCI_PORT_H__

#include <common/ia32/ports.h>

typedef u8_t	pci_bus_addr_t;
typedef u8_t	pci_dev_addr_t;
typedef u8_t	pci_func_addr_t;
typedef u8_t	pci_config_addr_t;

/* CONFIGURATION SPACE REGS */
#define PCI_CONFIG_ADDRESS 0xcf8  /* For addressing */
#define PCI_CONFIG_DATA    0xcfc  /* For io */
/* CONFIGURATION SPACE UTILS */
#define PCI_MAKE_CONFIG_ADDRESS_COMMAND(bus, device, fn, where)   (0x80000000 | (bus << 16) | (device << 11) | (fn << 8) | (where & ~3))
#define PCI_MAKE_CONFIG_DATA_OFFSET_8(offset) (PCI_CONFIG_DATA+(offset & 3))
#define PCI_MAKE_CONFIG_DATA_OFFSET_16(offset) (PCI_CONFIG_DATA+(offset & 2))
#define PCI_MAKE_CONFIG_DATA_OFFSET_32(offset) (PCI_CONFIG_DATA)

#define PCI_OPENDEV_BA_WORDTYPEMASK_VALID 1
#define PCI_OPENDEV_BA_WORDTYPEMASK_64    2

static inline u8_t pci_read8(pci_bus_addr_t bus, pci_dev_addr_t dev, pci_func_addr_t func, pci_config_addr_t off)
{
  out_u32(PCI_CONFIG_ADDRESS, PCI_MAKE_CONFIG_ADDRESS_COMMAND(bus, dev, func, off) );
  return in_u8( PCI_MAKE_CONFIG_DATA_OFFSET_8(off) );
}
static inline u16_t pci_read16(pci_bus_addr_t bus, pci_dev_addr_t dev, pci_func_addr_t func, pci_config_addr_t off)
{
  out_u32(PCI_CONFIG_ADDRESS, PCI_MAKE_CONFIG_ADDRESS_COMMAND(bus, dev, func, off) );
  return in_u16( PCI_MAKE_CONFIG_DATA_OFFSET_16(off) );
}
static inline u32_t pci_read32(pci_bus_addr_t bus, pci_dev_addr_t dev, pci_func_addr_t func, pci_config_addr_t off)
{
  out_u32(PCI_CONFIG_ADDRESS, PCI_MAKE_CONFIG_ADDRESS_COMMAND(bus, dev, func, off) );
  return in_u32( PCI_MAKE_CONFIG_DATA_OFFSET_32(off) );
}

static inline void pci_write8(pci_bus_addr_t bus, pci_dev_addr_t dev, pci_func_addr_t func, pci_config_addr_t off, u8_t value)
{
  out_u32(PCI_CONFIG_ADDRESS, PCI_MAKE_CONFIG_ADDRESS_COMMAND(bus, dev, func, off) );
  out_u8( PCI_MAKE_CONFIG_DATA_OFFSET_8(off) , value);
}
static inline void pci_write16(pci_bus_addr_t bus, pci_dev_addr_t dev, pci_func_addr_t func, pci_config_addr_t off, u16_t value)
{
  out_u32(PCI_CONFIG_ADDRESS, PCI_MAKE_CONFIG_ADDRESS_COMMAND(bus, dev, func, off) );
  out_u16( PCI_MAKE_CONFIG_DATA_OFFSET_16(off) , value);
}
static inline void pci_write32(pci_bus_addr_t bus, pci_dev_addr_t dev, pci_func_addr_t func, pci_config_addr_t off, u32_t value)
{
  out_u32(PCI_CONFIG_ADDRESS, PCI_MAKE_CONFIG_ADDRESS_COMMAND(bus, dev, func, off) );
  out_u32( PCI_MAKE_CONFIG_DATA_OFFSET_32(off) , value);
}

#endif /* __PCI__PCI_PORT_H__ */
