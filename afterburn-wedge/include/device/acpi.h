/*********************************************************************
 *                
 * Copyright (C) 2002-2008,  Karlsruhe University
 *                
 * File path:     device/acpi.h
 * Description:   ACPI structures for running on L4
 *                
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *                
 *                
 ********************************************************************/

#ifndef __L4KA__ACPI_H__
#define __L4KA__ACPI_H__

#if defined(CONFIG_DEVICE_APIC)

#include INC_ARCH(page.h)
#include INC_ARCH(types.h)
#include <debug.h>
#include <console.h>
#include INC_WEDGE(backend.h)
 
#define ACPI_MEM_SPACE	0
#define ACPI_IO_SPACE	1

#define ACPI20_PC99_RSDP_START	     0x0e0000
#define ACPI20_PC99_RSDP_END	     0x100000
#define ACPI20_PC99_RSDP_SIZELOG     (5 + PAGE_BITS)


class acpi_gas_t {
public:
    u8_t	id;
    u8_t	width;
    u8_t	offset;
private:
    u8_t	reserved;
    /* the 64-bit address is only 32-bit aligned */
    u32_t       addrlo;
    u32_t       addrhi;

public:
    u64_t address (void) { return (((u64_t) addrhi) << 32) + addrlo; }
} __attribute__((packed));


class acpi_thead_t {
public:
    u8_t	sig[4];
    u32_t	len;
    u8_t	rev;
    u8_t	csum;
    u8_t	oem_id[6];
    u8_t	oem_tid[8];
    u32_t	oem_rev;
    u32_t	creator_id;
    u32_t	creator_rev;
    
    bool is_entry(const char s[4])
	{
	    return (this->sig[0] == s[0] && this->sig[1] == s[1] && 
		    this->sig[2] == s[2] && this->sig[3] == s[3]);
	    
	}
    
} __attribute__((packed));

class acpi_madt_hdr_t {
public:
    u8_t	type;
    u8_t	len;
} __attribute__((packed));

class acpi_madt_lapic_t {
public:
    acpi_madt_hdr_t	header;
    u8_t		apic_processor_id;
    u8_t		id;
    struct {
	u32_t enabled	:  1;
	u32_t reserved	: 31;
    } flags;
} __attribute__((packed));

class acpi_madt_ioapic_t {
public:
    acpi_madt_hdr_t	header;
    u8_t	id;	  /* APIC id			*/
    u8_t	reserved;
    u32_t	address;  /* physical address		*/
    u32_t	irq_base; /* global irq number base	*/
} __attribute__((packed));


class acpi_madt_lsapic_t {
public:
    acpi_madt_hdr_t	header;
    u8_t		apic_processor_id;
    u8_t		id;
    u8_t		eid;
    u8_t		reserved[3];
    struct {
	u32_t enabled	:  1;
	u32_t		: 31;
    } flags;
} __attribute__((packed));


class acpi_madt_iosapic_t {
public:
    acpi_madt_hdr_t	header;
    u8_t	id;	  /* APIC id			*/
    u8_t	__reserved;
    u32_t	irq_base; /* global irq number base	*/
    u64_t	address;  /* physical address		*/
} __attribute__((packed));


class acpi_madt_irqovr_t {
public:
    acpi_madt_hdr_t	header;
    u8_t	src_bus;	/* source bus, fixed 0=ISA	*/
    u8_t	src_irq;	/* source bus irq		*/
    u32_t	dst_irq;	/* global irq number		*/
    union {
	u16_t	raw;		/* irq flags */
	struct {
	    u16_t polarity	: 2;
	    u16_t trigger_mode	: 2;
	    u16_t reserved	: 12;
	} x;
    } flags;

    enum polarity_t {
	conform_polarity = 0,
	active_high = 1,
	reserved_polarity = 2,
	active_low = 3
    };

    enum trigger_mode_t {
	conform_trigger = 0,
	edge = 1,
	reserved_trigger = 2,
	level = 3
    };
} __attribute__((packed));


class acpi_madt_nmi_t
{
public:
    acpi_madt_hdr_t	header;
    union {
	u16_t		raw;
	struct {
	    u16_t polarity	: 2;
	    u16_t trigger_mode	: 2;
	    u16_t reserved	: 12;
	} x;
    } flags;
    u32_t		irq;

    enum polarity_t {
	conform_polarity	= 0,
	active_high		= 1,
	active_low		= 3
    };

    enum trigger_mode_t {
	conform_trigger		= 0,
	edge			= 1,
	level			= 3
    };
    
};

class virtual_lapic_config_t  
{
public:
    word_t id;
    word_t cpu_id;
    word_t address;
    struct {
	u32_t enabled	:  1;
	u32_t configured:  1;
	u32_t reserved	: 30;
    } flags;
};
    
class virtual_ioapic_config_t
{
public:
    word_t id;
    word_t address;
    word_t irq_base;
    word_t max_redir_entry;
    struct {
	u32_t configured:  1;
	u32_t reserved	: 31;
    } flags;
};

/* 
 * virtual APIC configuration, set up by the wedge
 */

class virtual_apic_config_t
{
public:
    virtual_lapic_config_t lapic[CONFIG_NR_VCPUS];
    virtual_ioapic_config_t ioapic[CONFIG_MAX_IOAPICS];
};

class acpi_madt_t {
    acpi_thead_t header;
public:
    u32_t	local_apic_addr;
    u32_t	apic_flags;
private:
    u8_t	data[0];
    
    void recompute_checksum()
    {
	u8_t csum = 0;
	header.csum = 0;
	for (u32_t i=0; i < header.len; i++)
	    csum += ((u8_t*)this)[i];
	
	header.csum = 255 - csum + 1;
	
	ASSERT(checksum());
    }


public:
    bool checksum()
    {
	/* verify checksum */
	u8_t c = 0;
	for (u32_t i = 0; i < header.len; i++)
	    c += ((u8_t*)this)[i];
	if (c != 0)
	    return false;
	return true;
	}

    acpi_madt_hdr_t* find(u8_t type, int index) 
	{
	    for (word_t i = 0; i < (header.len-sizeof(acpi_madt_t));)
	    {
		acpi_madt_hdr_t* h = (acpi_madt_hdr_t*) &data[i];
		if (h->type == type)
		{
		    if (index == 0)
			return h;
		    index--;
		}
		i += h->len;
	    };
	    return NULL;
	}
    word_t len() { return header.len; }

    void copy(acpi_madt_t *to) 
    {
	
	ASSERT(header.len <= 4096);
	
	u8_t *src = (u8_t *) this;
	u8_t *dst = (u8_t *) to;

	for (word_t i = 0; i < header.len; i++)
	    dst[i] = src[i];
	
    };


private:
    void insert_entry(volatile u8_t *entry, u8_t entry_len)
    {
	word_t ofs = header.len - sizeof(acpi_madt_t);
	
	for (word_t i = 0; i < entry_len; i++)
	    data[i + ofs] = entry[i];
	
	header.len += entry_len;
	recompute_checksum();
    }
	    

public:
    acpi_madt_lapic_t*  lapic(int index) { return (acpi_madt_lapic_t*) find(0, index); };
    acpi_madt_ioapic_t* ioapic(int index) { return (acpi_madt_ioapic_t*) find(1, index); };
    acpi_madt_lsapic_t* lsapic(int index) { return (acpi_madt_lsapic_t*) find(7, index); };
    acpi_madt_iosapic_t* iosapic(int index) { return (acpi_madt_iosapic_t*) find (6, index); };
    acpi_madt_irqovr_t* irqovr(int index) { return (acpi_madt_irqovr_t*) find(2, index); };
    acpi_madt_nmi_t* nmi(int index) { return (acpi_madt_nmi_t*) find(3, index); };

public:
    void init_virtual() 
    {
	header.sig[0] = 'A'; 	
	header.sig[1] = 'P'; 	
	header.sig[2] = 'I'; 	
	header.sig[3] = 'C'; 	
	
	header.len = sizeof(acpi_madt_t);
	header.rev = 2;
	
	header.oem_id[0] = 'L';
	header.oem_id[1] = '4';
	header.oem_id[2] = 'K';
	header.oem_id[3] = 'A';
	header.oem_id[4] = 0;
	header.oem_id[5] = 0;
	    
	
	header.oem_tid[0] = 'P';
	header.oem_tid[1] = 'R'; 
	header.oem_tid[2] = 'E'; 
	header.oem_tid[3] = 'V'; 
	header.oem_tid[4] = 'I'; 
	header.oem_tid[5] = 'R'; 
	header.oem_tid[6] = 'T'; 
	header.oem_tid[7] = 0;
	
	header.oem_rev = 1;
	
	local_apic_addr = 0;
	apic_flags = 0;
	
	recompute_checksum();
    }
	
    void insert_lapic(virtual_lapic_config_t vlapic_config, word_t current_cpu_id)
    {
	if (vlapic_config.flags.configured == 0)
	    return;

	acpi_madt_lapic_t nlapic; 
	
	nlapic.header.type = 0;
	nlapic.header.len = sizeof(acpi_madt_lapic_t);
	nlapic.apic_processor_id = vlapic_config.cpu_id;
	nlapic.id = vlapic_config.id;
	ASSERT(vlapic_config.id < CONFIG_NR_VCPUS);
	nlapic.flags.enabled = vlapic_config.flags.enabled;
	nlapic.flags.reserved = 0;

	insert_entry((u8_t *) &nlapic, sizeof(acpi_madt_lapic_t));
		
	if (vlapic_config.cpu_id == current_cpu_id)
	    local_apic_addr = vlapic_config.address;

    }

    void insert_ioapic(virtual_ioapic_config_t vioapic_config)
    {
	if (vioapic_config.flags.configured == 0)
	    return;

	acpi_madt_ioapic_t nioapic; 
	nioapic.header.type = 1;
	nioapic.header.len = sizeof(acpi_madt_ioapic_t);
	nioapic.id = vioapic_config.id;
	nioapic.reserved = 0;
	nioapic.address = vioapic_config.address;
	nioapic.irq_base = vioapic_config.irq_base;

	insert_entry((u8_t *) &nioapic, sizeof(acpi_madt_ioapic_t));
	
    }
    
    void insert_irqovr(word_t src_irq, word_t dst_irq, word_t polarity, word_t trigger_mode)
    {
       	acpi_madt_irqovr_t nirqovr; 
	nirqovr.header.type = 2;
	nirqovr.header.len = sizeof(acpi_madt_irqovr_t);
	nirqovr.src_bus = 0;
	nirqovr.src_irq = src_irq;
	nirqovr.dst_irq = dst_irq;
	nirqovr.flags.raw = 0;
	nirqovr.flags.x.polarity = polarity;
	nirqovr.flags.x.trigger_mode = trigger_mode;
	
	insert_entry((u8_t *) &nirqovr, sizeof(acpi_madt_irqovr_t));

    }	
    
    void insert_nmi(word_t irq, word_t polarity, word_t trigger_mode)
    {
	printf( "insert nmi");
	acpi_madt_nmi_t nnmi;
	nnmi.header.type = 3;
	nnmi.header.len = sizeof(acpi_madt_nmi_t);
	nnmi.flags.raw = 0;
	nnmi.flags.x.polarity = polarity;
	nnmi.flags.x.trigger_mode = trigger_mode;
	nnmi.irq = irq;
	    
	insert_entry((u8_t *) &nnmi, sizeof(acpi_madt_nmi_t));
    }
	
    
} __attribute__((packed));

/*
  RSDT and XSDT differ in their pointer size only
  rsdt: 32bit, xsdt: 64bit
*/
template<typename T> class acpi__sdt_t {
    acpi_thead_t	header;
    T			ptrs[0];

private:
    void recompute_checksum()
    {
	
	u8_t csum = 0;
	header.csum = 0;
	for (u32_t i=0; i < header.len; i++)
	    csum += ((u8_t*)this)[i];
	
	header.csum = 255 - csum + 1;
	
	ASSERT(checksum());
    }

public:
    bool checksum()
	{
	/* verify checksum */
	u8_t c = 0;
	for (u32_t i = 0; i < header.len; i++)
	    c += ((u8_t*)this)[i];
	if (c != 0)
	    return false;
	return true;
	}

    acpi_thead_t* get_entry(word_t &idx)
    {
	word_t rwx = 7;
	if (idx >= ((header.len-sizeof(header))/sizeof(ptrs[0])))
	    return NULL;

	if (((word_t) this & PAGE_MASK) != ((word_t) ptrs[idx] & PAGE_MASK))
	    backend_request_device_mem(ptrs[idx], PAGE_SIZE, rwx);
	
	return  (acpi_thead_t*)((word_t)ptrs[idx++]);
	
    }
    
    /* find table with a given signature */
    acpi_thead_t* get_entry(const char sig[4], word_t last_virtual_byte) 
	{
	    for (word_t i = 0; i < ((header.len-sizeof(header))/sizeof(ptrs[0])); i++)
	    {
		if ((word_t)ptrs[i] > last_virtual_byte)
		    continue;		

		word_t rwx = 7;

		if (((word_t) this & PAGE_MASK) != ((word_t) ptrs[i] & PAGE_MASK))
		    backend_request_device_mem(ptrs[i], PAGE_SIZE, rwx);
		
		acpi_thead_t* t= (acpi_thead_t*)((word_t)ptrs[i]);
		
		if (t->is_entry(sig))
		    return t;
		
		if (((word_t) this & PAGE_MASK) !=  ((word_t) ptrs[i] & PAGE_MASK))
		    backend_unmap_device_mem(ptrs[i], PAGE_SIZE, rwx);

	    };
	    return NULL;
	};
    

    void set_entry(const char sig[4], word_t nptr, word_t last_virtual_byte) 
	{
	    for (word_t i = 0; i < ((header.len-sizeof(header))/sizeof(ptrs[0])); i++)
	    {
		if ((word_t)ptrs[i] > last_virtual_byte)
		    continue;		

		acpi_thead_t* t= (acpi_thead_t*)((word_t)ptrs[i]);
		
		if (t->sig[0] == sig[0] && 
			t->sig[1] == sig[1] && 
			t->sig[2] == sig[2] && 
			t->sig[3] == sig[3])
		{
		    ptrs[i] = nptr;
		    recompute_checksum();			
		    return; 
		}
		
	    };
	};


    word_t len() { return header.len; };

    void copy(acpi__sdt_t *to) 
	{
	    
	    ASSERT(header.len <= 4096);
 
	    u8_t *src = (u8_t *) this;
	    u8_t *dst = (u8_t *) to;

	    for (word_t i = 0; i < header.len; i++)
		dst[i] = src[i];

	};

    void init_virtual() 
    {
	header.oem_id[0] = 'L';
	header.oem_id[1] = '4';
	header.oem_id[2] = 'K';
	header.oem_id[3] = 'A';
	header.oem_id[4] = 0;
	header.oem_id[5] = 0;
	    
	header.oem_tid[0] = 'P';
	header.oem_tid[1] = 'R'; 
	header.oem_tid[2] = 'E'; 
	header.oem_tid[3] = 'V'; 
	header.oem_tid[4] = 'I'; 
	header.oem_tid[5] = 'R'; 
	header.oem_tid[6] = 'T'; 
	header.oem_tid[7] = 0;
	
	recompute_checksum();
	
    }
    
    friend class kdb_t;
} __attribute__((packed));

typedef acpi__sdt_t<u32_t> acpi_rsdt_t;
typedef acpi__sdt_t<u64_t> acpi_xsdt_t;

 
class acpi_rsdp_t {
    
    u8_t	sig[8];
    u8_t	csum;
    u8_t	oem_id[6];
    u8_t	rev;
    u32_t	rsdt_ptr;
    u32_t	rsdp_len;
    u64_t	xsdt_ptr;
    u8_t	xcsum;
    u8_t	reserved[3];
private:
    void recompute_checksum()
    {
	
	u8_t c = 0;
	csum = 0;
	for (u32_t i=0; i < 20; i++)
	    c += ((u8_t*)this)[i];
	
	csum = 255 - c + 1;

	xcsum = c = 0;
	for (u8_t i=0; i < sizeof(acpi_rsdp_t); i++)
	    c += ((u8_t*)this)[i];
	xcsum = 255 - c + 1;
	ASSERT(checksum());
	
    }
public:
    static acpi_rsdp_t* locate();
   
    bool checksum()
    {
	/* verify checksum */
	u8_t c = 0;
	for (u32_t i = 0; i < 20; i++)
	    c += ((u8_t*)this)[i];
	if (c != 0)
	    return false;
	
#if 0
	/* verify extended checksum  */
	for (u8_t i=0; i < sizeof(acpi_rsdp_t); i++)
	    c += ((u8_t*)this)[i];
	if (c != 0)
	    return false;
#endif	
	return true;
	
    }

    void init_virtual() 
    {
	oem_id[0] = 'L';
	oem_id[1] = '4';
	oem_id[2] = 'K';
	oem_id[3] = 'A';
	oem_id[4] = 0;
	oem_id[5] = 0;
	    
	rsdp_len = sizeof(acpi_rsdp_t);
	recompute_checksum();
    }
    
    acpi_rsdt_t* get_rsdt() {
	return (acpi_rsdt_t*) (word_t)rsdt_ptr;
    };

    void set_rsdt(word_t nrsdt) {
	rsdt_ptr = nrsdt;
	recompute_checksum();
    };

    acpi_xsdt_t* get_xsdt() {
	
	/* check version - only ACPI 2.0 knows about an XSDT */
	if (rev != 2)
	    return NULL;
	
	return (acpi_xsdt_t*) (word_t)xsdt_ptr;
    };
    
    void set_xsdt(word_t nxsdt) {
	xsdt_ptr = nxsdt;
	recompute_checksum();
    };

    word_t len() { return rsdp_len; };

    void copy(acpi_rsdp_t *to) 
	{
	    u8_t *src = (u8_t *) this;
	    u8_t *dst = (u8_t *) to;
	    
	    for (word_t i = 0; i < sizeof(acpi_rsdp_t); i++)
		dst[i] = src[i];
	};
    
	
} __attribute__((packed));


/* 
 * virtual RSDT/XSDT, RSDP, MADT
 */

class acpi_t
{
private: 
    acpi_rsdp_t* rsdp;
    acpi_rsdt_t* rsdt;
    acpi_xsdt_t* xsdt;
    acpi_madt_t* madt;

    word_t lapic_base;
    
    struct{
	word_t id;

	word_t irq_base;
	word_t address;
    } ioapic[CONFIG_MAX_IOAPICS];
    u32_t nr_ioapics;

    /* 
     * virtual RSDP, RSDT, XSDT, MADT
     * jsTODO: avoid waste of space by not relying on page boundaries
     */
    u32_t __virtual_rsdp[CONFIG_NR_VCPUS][1024] __attribute__((aligned(4096)));
    u32_t __virtual_madt[CONFIG_NR_VCPUS][1024] __attribute__((aligned(4096)));
    u32_t __virtual_rsdt[CONFIG_NR_VCPUS][1024] __attribute__((aligned(4096)));
    u32_t __virtual_xsdt[CONFIG_NR_VCPUS][1024] __attribute__((aligned(4096)));

    acpi_rsdp_t *virtual_rsdp[CONFIG_NR_VCPUS];
    acpi_rsdt_t *virtual_rsdt[CONFIG_NR_VCPUS];
    acpi_xsdt_t *virtual_xsdt[CONFIG_NR_VCPUS];
    acpi_madt_t *virtual_madt[CONFIG_NR_VCPUS];

    word_t virtual_rsdp_phys[CONFIG_NR_VCPUS];
    word_t virtual_rsdt_phys[CONFIG_NR_VCPUS];
    word_t virtual_xsdt_phys[CONFIG_NR_VCPUS];
    word_t virtual_madt_phys[CONFIG_NR_VCPUS];

    /* 
     * virtual EBDA, current empty
     */
    u32_t __virtual_ebda[1024] __attribute__((aligned(4096)));
    word_t virtual_ebda;
    word_t bios_ebda;
    
    static word_t virtual_table_phys(word_t virtual_table)
	{
	    word_t dummy;
	    return backend_resolve_addr((word_t) virtual_table, dummy)->get_address() +
		(virtual_table & ~PAGE_MASK);
	}	
	
public:
	
    acpi_t () 
	{
	    for (word_t vcpu=0; vcpu < CONFIG_NR_VCPUS; vcpu++)
	    {
		virtual_rsdp[vcpu] = (acpi_rsdp_t *) &__virtual_rsdp[vcpu][0];
		virtual_rsdt[vcpu] = (acpi_rsdt_t *) &__virtual_rsdt[vcpu][0];
		virtual_xsdt[vcpu] = (acpi_xsdt_t *) &__virtual_xsdt[vcpu][0];
		virtual_madt[vcpu] = (acpi_madt_t *) &__virtual_madt[vcpu][0];

	    }
	    virtual_ebda = (word_t) __virtual_ebda;
	    rsdp = NULL;
	    rsdt = NULL;
	    xsdt = NULL;
	    madt = NULL;
	    nr_ioapics = 0;
	}
    

    void init()
	{
	    word_t rwx = 7;

	    word_t last_virtual_byte = 0;

	    L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *)L4_GetKernelInterface();

	    L4_Word_t num_mdesc = L4_NumMemoryDescriptors( kip );

	    /* First search for end of virtual address space */
	    for( L4_Word_t idx = 0; idx < num_mdesc; idx++ )
	    {
		L4_MemoryDesc_t *mdesc = L4_MemoryDesc( kip, idx );
		L4_Word_t end = L4_MemoryDescHigh(mdesc);

		if ( L4_IsVirtual(mdesc) && 
		     L4_MemoryDescType(mdesc) == L4_ConventionalMemoryType )
		    if (last_virtual_byte < end)
			last_virtual_byte = end;
	    }

	    /* Hardcode to 3GB if search failed */
	    if ( last_virtual_byte == 0 )
		last_virtual_byte = 0xc0000000 - 1;
	    
	    backend_request_device_mem((word_t) 0x40E, PAGE_SIZE, rwx, true);
	    bios_ebda = (*(L4_Word16_t *) 0x40E) * 16;
	    backend_unmap_device_mem((word_t) 0x40E, PAGE_SIZE, rwx, true);
	    
	    if (!bios_ebda)
		bios_ebda = 0x9fc00;
	    
	    dprintf(debug_acpi, "ACPI EBDA  @ %x (virtual %x)\n", bios_ebda, virtual_ebda);

    
	    if ((rsdp = acpi_rsdp_t::locate()) == NULL)
	    {
		printf( "ACPI RSDP table not found\n");
		return;
	    }
	    
	    	    
	    if (!rsdp->checksum())
	    {
		printf( "ACPI RSDP checksum incorrect\n");
		DEBUGGER_ENTER("ACPI BUG");
		return;
	    }
	    dprintf(debug_acpi, "ACPI RSDP  @ %x\n", rsdp);
	
	    rsdt = rsdp->get_rsdt();
	    xsdt = rsdp->get_xsdt();
		
	    if (debug_acpi)
	    {
		dprintf(debug_acpi, "ACPI RSDT  @ %x\n", rsdt);
		dprintf(debug_acpi, "ACPI XSDT  @ %x\n", xsdt);
	    }


	    if (xsdt == NULL && rsdt == NULL)
	    {
		printf( "ACPI neither RSDT nor XSDT found\n"); 
		return;
	    }

	    acpi_thead_t* pt;

	    if (xsdt != NULL)
	    {
		backend_request_device_mem((word_t) xsdt, PAGE_SIZE, rwx, true);
		madt = (acpi_madt_t*) xsdt->get_entry("APIC", last_virtual_byte);
	    }
	    
	    if ((madt == NULL) && (rsdt != NULL))
	    {
		backend_request_device_mem((word_t) rsdt, PAGE_SIZE, rwx, true);
		madt = (acpi_madt_t*) rsdt->get_entry("APIC", last_virtual_byte);
		
		word_t idx = 0;
		while ((pt = rsdt->get_entry(idx)))
		    dprintf(debug_acpi, "ACPI %c%c%c%c @ %x (passthrough)\n",
			    (char) pt->sig[0], (char) pt->sig[1],
			    (char) pt->sig[2], (char) pt->sig[3], pt);
		
		/* yet unimplemented */

	    }
	
	    dprintf(debug_acpi, "ACPI MADT  @ %x\n", madt);
	    dprintf(debug_acpi, "ACPI LAPIC @ %x\n", madt->local_apic_addr);
	    
	    lapic_base = madt->local_apic_addr;

	    for (word_t i = 0; ((madt->ioapic(i)) != NULL && i < CONFIG_MAX_IOAPICS); i++)

	    {
		ioapic[i].id = madt->ioapic(i)->id;
 		ioapic[i].irq_base = madt->ioapic(i)->irq_base;
		ioapic[i].address = madt->ioapic(i)->address;
		
		dprintf(debug_acpi, "ACPI IOAPIC id %d base %x @ %x\n", ioapic[i].id,
			ioapic[i].irq_base, ioapic[i].address);
		
		nr_ioapics++;	
	    }
	    
	    /*
	     * Build virtual ACPI tables from physical ACPI tables
	     */
	    
	    for (word_t vcpu=0; vcpu < vcpu_t::nr_vcpus; vcpu++)
	    {
		virtual_rsdp_phys[vcpu] = virtual_table_phys((word_t) virtual_rsdp[vcpu]);
		virtual_rsdt_phys[vcpu] = virtual_table_phys((word_t) virtual_rsdt[vcpu]);
		virtual_xsdt_phys[vcpu] = virtual_table_phys((word_t) virtual_xsdt[vcpu]);
		virtual_madt_phys[vcpu] = virtual_table_phys((word_t) virtual_madt[vcpu]);
		
		if (rsdt)
		{
		    dprintf(debug_acpi, "ACPI creating virtual RSDT for VCPU %d %x (phys %x)\n",
			    vcpu, virtual_rsdt[vcpu], virtual_rsdt_phys[vcpu]);
		    
		    rsdt->copy(virtual_rsdt[vcpu]);	
	    
		    dprintf(debug_acpi, "ACPI patching virtual MADT pointer for VCPU %d into RSDT\n", vcpu);
		    
		    virtual_rsdt[vcpu]->init_virtual();
		    virtual_rsdt[vcpu]->set_entry("APIC", virtual_madt_phys[vcpu], last_virtual_byte);
		    
		    dprintf(debug_acpi, "ACPI creating virtual RSDP for VCPU %d %x (phys %x)\n",
			    vcpu, virtual_rsdp[vcpu], virtual_rsdp_phys[vcpu]);
		    
		    rsdp->copy(virtual_rsdp[vcpu]);
		    virtual_rsdp[vcpu]->init_virtual();
		    

		    dprintf(debug_acpi, "ACPI patching virtual RSDT pointer for VCPU %d into RSDP\n", vcpu);
		    virtual_rsdp[vcpu]->set_rsdt(virtual_rsdt_phys[vcpu]);
		}
		if (xsdt)
		{
		    dprintf(debug_acpi, "ACPI creating virtual XSDT for VCPU %d %x (phys %x)\n",
			    vcpu, virtual_xsdt[vcpu], virtual_xsdt_phys[vcpu]);
		    xsdt->copy(virtual_xsdt[vcpu]);
		    
		    dprintf(debug_acpi, "ACPI patching virtual MADT pointer for VCPU %d into RSDT\n", vcpu);
		    
		    virtual_xsdt[vcpu]->init_virtual();
		    virtual_xsdt[vcpu]->set_entry("APIC", virtual_madt_phys[vcpu], last_virtual_byte);	
		    
		    if (!rsdt)
		    {
		    
			dprintf(debug_acpi, "ACPI creating virtual RSDP for VCPU %d %x (phys %x)\n",
				vcpu, virtual_rsdp[vcpu], virtual_rsdp_phys[vcpu]);
			rsdp->copy(virtual_rsdp[vcpu]);
			virtual_rsdp[vcpu]->init_virtual();
		    }
		    
		    dprintf(debug_acpi, "ACPI patching virtual XSDT pointer for VCPU %d into RSDP\n", vcpu);
		    virtual_rsdp[vcpu]->set_rsdt(virtual_rsdt_phys[vcpu]);
		    virtual_rsdp[vcpu]->set_xsdt(virtual_xsdt_phys[vcpu]);
		    
		}
		

	    }
	    dprintf(debug_acpi, "ACPI initialized\n");
	    
	    if (rsdp)
		backend_unmap_device_mem((word_t) rsdp, PAGE_SIZE, rwx, true);
	    if (xsdt)
		backend_unmap_device_mem((word_t) xsdt, PAGE_SIZE, rwx, true);
	    if (rsdt)
		backend_unmap_device_mem((word_t) rsdt, PAGE_SIZE, rwx, true);
	    if (madt)
		backend_unmap_device_mem((word_t) madt, PAGE_SIZE, rwx, true);


	}
    

    bool handle_pfault(thread_info_t *ti, map_info_t &map_info, word_t &paddr, bool &nilmapping);
    bool is_acpi_table(word_t paddr);
	
    u32_t get_nr_ioapics() { return nr_ioapics; }
	
    u32_t get_ioapic_irq_base(u32_t idx) { 

	ASSERT(idx < CONFIG_MAX_IOAPICS);
	return ioapic[idx].irq_base;
    }

    u32_t get_ioapic_id(u32_t idx) { 

	ASSERT(idx < CONFIG_MAX_IOAPICS);
	return ioapic[idx].id;
    }
	
    u32_t get_ioapic_addr(u32_t idx) { 

	ASSERT(idx < CONFIG_MAX_IOAPICS);
	return ioapic[idx].address;
    }
	
    void init_virtual_madt(const virtual_apic_config_t vapic_config, const word_t num_vcpus)
	{
	    
	    //madt->copy(virtual_madt);

	    for (word_t vcpu=0; vcpu < num_vcpus; vcpu++)
	    {
		dprintf(debug_acpi, "ACPI creating virtual MADT for VCPU %d %x (phys %x)\n",
			vcpu, virtual_madt[vcpu] , virtual_madt_phys[vcpu]);
		
		virtual_madt[vcpu]->init_virtual();
		
		for (word_t vlapic=0; vlapic < num_vcpus; vlapic++)
		    virtual_madt[vcpu]->insert_lapic(vapic_config.lapic[vlapic], vapic_config.lapic[vcpu].cpu_id);

		for (word_t vioapic=0; vioapic < CONFIG_MAX_IOAPICS; vioapic++)
		    virtual_madt[vcpu]->insert_ioapic(vapic_config.ioapic[vioapic]);
		
		/*
		 * jsXXX: this assumes 
		 *        IRQ override from ISA IRQ0 to GSI 2 active high, edge triggered
		 */
		virtual_madt[vcpu]->insert_irqovr(0, 2, 1, 1);
	    }
	}		
		    
		    
};

extern acpi_t acpi;

#endif /* defined(CONFIG_DEVICE_APIC) */


#endif /* __L4KA__ACPI_H__ */
