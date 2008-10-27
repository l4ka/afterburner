#include "vl.h"

#define BITS_PER_LONG (sizeof(long)*8)
#define BITS_TO_LONGS(bits) \
    (((bits)+BITS_PER_LONG-1)/BITS_PER_LONG)
#define DECLARE_BITMAP(name,bits) \
unsigned long name[BITS_TO_LONGS(bits)]

#define hvm_pci_intx_link(dev, intx) \
    (((dev) + (intx)) & 3)


struct hvm_hw_pci_irqs {
      /*
       * Virtual interrupt wires for a single PCI bus.
       * Indexed by: device*4 + INTx#.
       */
      union {
	  DECLARE_BITMAP(i, 32*4);
	  uint64_t pad[2];
      };
};

struct hvm_hw_isa_irqs {
    /*
     * Virtual interrupt wires for ISA devices.
     * Indexed by ISA IRQ (assumes no ISA-device IRQ sharing).
     */
    union {
        DECLARE_BITMAP(i, 16);
        uint64_t pad[1];
    };
};

struct hvm_hw_pci_link {
    /*
     * PCI-ISA interrupt router.
     * Each PCI <device:INTx#> is 'wire-ORed' into one of four links using
     * the traditional 'barber's pole' mapping ((device + INTx#) & 3).
     * The router provides a programmable mapping from each link to a GSI.
     */
    uint8_t route[4];
    uint8_t pad0[4];
};

struct hvm_irq {

    /*
     * Virtual interrupt wires for a single PCI bus.
     * Indexed by: device*4 + INTx#.
     */
    struct hvm_hw_pci_irqs pci_intx;

    /*                                                                                                                                                                            
     * Virtual interrupt wires for ISA devices.
     * Indexed by ISA IRQ (assumes no ISA-device IRQ sharing).
     */
    struct hvm_hw_isa_irqs isa_irq;

    /*
     * PCI-ISA interrupt router.
     * Each PCI <device:INTx#> is 'wire-ORed' into one of four links using
     * the traditional 'barber's pole' mapping ((device + INTx#) & 3).
     * The router provides a programmable mapping from each link to a GSI.
     */
    struct hvm_hw_pci_link pci_link;

    /* Number of INTx wires asserting each PCI-ISA link. */
    uint8_t pci_link_assert_count[4];


};
