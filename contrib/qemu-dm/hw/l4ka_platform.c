/*
 * XEN platform fake pci device, formerly known as the event channel device
 * 
 * Copyright (c) 2003-2004 Intel Corp.
 * Copyright (c) 2006 XenSource
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/*
 * L4ka pci and irq emulation code
 */
#include "vl.h"
#include "bitops.h"
#include "l4ka_irq.h"

//#include <xenguest.h>



struct hvm_irq l4ka_irq;

extern FILE *logfile;

static void platform_ioport_map(PCIDevice *pci_dev, int region_num,
                                uint32_t addr, uint32_t size, int type)
{
    /* nothing yet */
}

static uint32_t platform_mmio_read(void *opaque, target_phys_addr_t addr)
{
    static int warnings = 0;
    if (warnings < 5) {
        fprintf(logfile, "Warning: attempted read from physical address "
                "0x%"PRIx64" in l4ka platform mmio space\n", (uint64_t)addr);
        warnings++;
    }
    return 0;
}

static void platform_mmio_write(void *opaque, target_phys_addr_t addr,
                                uint32_t val)
{
    static int warnings = 0;
    if (warnings < 5) {
        fprintf(logfile, "Warning: attempted write of 0x%x to physical "
                "address 0x%"PRIx64" in l4ka platform mmio space\n",
                val, (uint64_t)addr);
        warnings++;
    }
    return;
}

static CPUReadMemoryFunc *platform_mmio_read_funcs[3] = {
    platform_mmio_read,
    platform_mmio_read,
    platform_mmio_read,
};

static CPUWriteMemoryFunc *platform_mmio_write_funcs[3] = {
    platform_mmio_write,
    platform_mmio_write,
    platform_mmio_write,
};

static void platform_mmio_map(PCIDevice *d, int region_num,
                              uint32_t addr, uint32_t size, int type)
{
    int mmio_io_addr;

    mmio_io_addr = cpu_register_io_memory(0, platform_mmio_read_funcs,
                                          platform_mmio_write_funcs, NULL);

    cpu_register_physical_memory(addr, 0x1000000, mmio_io_addr);
}

struct pci_config_header {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t  revision;
    uint8_t  api;
    uint8_t  subclass;
    uint8_t  class;
    uint8_t  cache_line_size; /* Units of 32 bit words */
    uint8_t  latency_timer; /* In units of bus cycles */
    uint8_t  header_type; /* Should be 0 */
    uint8_t  bist; /* Built in self test */
    uint32_t base_address_regs[6];
    uint32_t reserved1;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t rom_addr;
    uint32_t reserved3;
    uint32_t reserved4;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint8_t  min_gnt;
    uint8_t  max_lat;
};

void l4ka_pci_save(QEMUFile *f, void *opaque)
{
    PCIDevice *d = opaque;

    pci_device_save(d, f);
}

int l4ka_pci_load(QEMUFile *f, void *opaque, int version_id)
{
    PCIDevice *d = opaque;

    if (version_id != 1)
        return -EINVAL;

    return pci_device_load(d, f);
}

void pci_l4ka_platform_init(PCIBus *bus)
{
    PCIDevice *d;
    struct pci_config_header *pch;

    printf("Register l4ka platform.\n");
    d = pci_register_device(bus, "l4ka-platform", sizeof(PCIDevice), -1, NULL,
			    NULL);
    pch = (struct pci_config_header *)d->config;
    pch->vendor_id = 0x5853;
    pch->device_id = 0x0001;
    pch->command = 3; /* IO and memory access */
    pch->revision = 1;
    pch->api = 0;
    pch->subclass = 0x80; /* Other */
    pch->class = 0xff; /* Unclassified device class */
    pch->header_type = 0;
    pch->interrupt_pin = 1;

    /* Microsoft WHQL requires non-zero subsystem IDs. */
    /* http://www.pcisig.com/reflector/msg02205.html.  */
    pch->subsystem_vendor_id = pch->vendor_id; /* Duplicate vendor id.  */
    pch->subsystem_id        = 0x0001;         /* Hardcode sub-id as 1. */

    pci_register_io_region(d, 0, 0x100, PCI_ADDRESS_SPACE_IO,
                           platform_ioport_map);

    /* reserve 16MB mmio address for share memory*/
    pci_register_io_region(d, 1, 0x1000000, PCI_ADDRESS_SPACE_MEM_PREFETCH,
			   platform_mmio_map);

    register_savevm("platform", 0, 1, l4ka_pci_save, l4ka_pci_load, d);
    fprintf(logfile,"Done register platform.\n");
}

/* Programmable Interrupt Router Emulation */

static void l4ka_pci_intx_assert( uint8_t device, uint8_t intx)
{
    struct hvm_irq *hvm_irq = &l4ka_irq;
    unsigned int link, isa_irq;

    if((device > 31) && (intx > 3))
    {
	fprintf(logfile,"Invalid pci device or intx value. device %d, intx %d\n",device,intx);
	exit(1);
    }
    
    if ( __test_and_set_bit(device*4 + intx, &hvm_irq->pci_intx.i) )
        return;

    link    = hvm_pci_intx_link(device, intx);
    isa_irq = hvm_irq->pci_link.route[link];
    if ( (hvm_irq->pci_link_assert_count[link]++ == 0) && isa_irq)
    {
//        vioapic_irq_positive_edge(d, isa_irq);
        pic_set_irq(isa_irq,1);
    }
}

static void l4ka_pci_intx_deassert(uint8_t device, uint8_t intx)
{
    struct hvm_irq *hvm_irq = &l4ka_irq;
    unsigned int link, isa_irq;

    if((device > 31) && (intx > 3))
    {
	fprintf(logfile,"Invalid pci device or intx value in pci_intx_deassert. device %d, intx %d\n",device,intx);
	exit(1);
    }

    if ( !__test_and_clear_bit(device*4 + intx, &hvm_irq->pci_intx.i) )
        return;

/*    gsi = hvm_pci_intx_gsi(device, intx);
      --hvm_irq->gsi_assert_count[gsi];*/

    link    = hvm_pci_intx_link(device, intx);
    isa_irq = hvm_irq->pci_link.route[link];
    if ( (--hvm_irq->pci_link_assert_count[link] == 0) && isa_irq)
        pic_set_irq(isa_irq,0);
}


void l4ka_set_pci_intx_level(uint8_t  device, uint8_t intx, uint8_t level)
{

    switch ( level )
    {
	case 0:
	    l4ka_pci_intx_deassert(device, intx);
	    break;
	case 1:
	    l4ka_pci_intx_assert(device, intx);
	    break;
	default:
	    fprintf(logfile,"Invalid pci intx line level %d, intx %d, device %d\n",level,intx,device);
	    break;
    }

}

extern void l4ka_raise_irq(int irq);
//TODO send one IPC with irq mask instead of sending one IPC for every pending irq
void l4ka_raise_pending_isa_irq(void)
{
    l4ka_raise_irq(l4ka_irq.isa_irq.i[0]);
}

void init_irq_logic(void)
{
    int i;
    l4ka_irq.pci_intx.pad[0] = 0;
    l4ka_irq.pci_intx.pad[0] = 0;

    l4ka_irq.isa_irq.pad[0] = 0;

    for(i = 0; i < 4;i++)
    {
	l4ka_irq.pci_link.route[i] = 0;
	l4ka_irq.pci_link.pad0[i] = 0;
    }
    
}

void l4ka_set_pci_link_route(uint8_t link, uint8_t isa_irq)
{
    struct hvm_irq *hvm_irq = &l4ka_irq;
    uint8_t old_isa_irq;

    if((link >= 4) && (isa_irq >= 16))
	printf("invalid link or isa irq in set_pci_link_route. link %d, isa_irq %d\n",link,isa_irq);

    old_isa_irq = hvm_irq->pci_link.route[link];
    if ( old_isa_irq == isa_irq )
        goto out;
    hvm_irq->pci_link.route[link] = isa_irq;


out:
    fprintf(logfile, "PCI link %u changed %u -> %u\n",
            link, old_isa_irq, isa_irq);
}
