/*********************************************************************
 *                
 * Copyright (C) 2003, 2005-2008,  Karlsruhe University
 *                
 * File path:     acpi.cc
 * Description:   ACPI support code for IA-PCs (PC99)
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
 * $Id: acpi.cc,v 1.2 2006/01/11 17:40:40 store_mrs Exp $
 *                
 ********************************************************************/
#ifndef __PLATFORM__PC99__ACPI_H__
#define __PLATFORM__PC99__ACPI_H__

#include INC_WEDGE(backend.h)
#include <device/acpi.h>
#include <device/lapic.h>

#include <../contrib/rombios/acpi-dsdt.hex>

/* 
 * ACPI 2.0 Specification, 5.2.4.1
 * Finding the RSDP on IA-PC Systems 
 */

word_t acpi_rsdp_t::locate_phys(word_t &map_base)
{
    /** @todo checksum, check version */
    for (word_t phys = ACPI20_PC99_RSDP_START;
	 phys < ACPI20_PC99_RSDP_END;
	 phys = phys + 4096)
    {

	//jsXXX: check if real device mem and ignore if so
	map_base = remap_acpi_mem(phys, map_base);
	
	for (word_t *p = (word_t *) map_base; p < (word_t *) (map_base + 4096) ; p+=4)
	{
	    acpi_rsdp_t* r = (acpi_rsdp_t*) p;
	    if (r->sig[0] == 'R' &&
		    r->sig[1] == 'S' &&
		    r->sig[2] == 'D' &&
		    r->sig[3] == ' ' &&
		    r->sig[4] == 'P' &&
		    r->sig[5] == 'T' &&
		    r->sig[6] == 'R' &&
		    r->sig[7] == ' ')
	    {
		map_base += ((word_t) p & ~PAGE_MASK);
		return (phys + ((word_t) p & ~PAGE_MASK));
	    }
	}
	unmap_acpi_mem(phys);
    };
    /* not found */
    return NULL;
};

bool acpi_t::handle_pfault(thread_info_t *ti, map_info_t &map_info, word_t &paddr, bool &nilmapping)
{
    vcpu_t &vcpu = get_vcpu();
    word_t new_vaddr;
    
    if (paddr == bios_ebda)
    {
	dprintf(debug_acpi, "EBDA %x remap to %x\n", bios_ebda, virtual_ebda);
	new_vaddr = virtual_ebda;
	nilmapping = true;
	goto acpi_pfault;
    }
    
    paddr &= PAGE_MASK;
    
    /* 
     * If the guest tries to access SMP tables, we poke in a zero page, even on
     * passthrough acces. We don't want it to discover real CPUs
     */
    if (paddr >= ACPI20_PC99_RSDP_START &&
	    paddr <= ACPI20_PC99_RSDP_END)
    {
	if (paddr == ACPI20_PC99_RSDP_START)
	{
	    new_vaddr = (word_t) virtual_rsdp[vcpu.cpu_id];
	    nilmapping = false;
	    goto acpi_pfault;
	}
	return false;
    
    }
    if (paddr == virtual_rsdt_phys[vcpu.cpu_id])
    {
	new_vaddr = (word_t) virtual_rsdt[vcpu.cpu_id];
	nilmapping = false;
	goto acpi_pfault;
    }
    
    if (paddr == virtual_madt_phys[vcpu.cpu_id])
    {
	new_vaddr = (word_t) virtual_madt[vcpu.cpu_id];
	nilmapping = false;
	goto acpi_pfault;
    }

    if (paddr == virtual_xsdt_phys[vcpu.cpu_id])
    {
	new_vaddr = (word_t) virtual_xsdt[vcpu.cpu_id];
	nilmapping = false;
	goto acpi_pfault;
    }
    
    return false;
    
 acpi_pfault:
    
    idl4_fpage_t fp;
    L4_Fpage_t fp_recv, fp_req;
    CORBA_Environment ipc_env = idl4_default_environment;
    word_t link_addr = vcpu.get_kernel_vaddr();

    dprintf(debug_acpi, "ACPI table override phys %x new %x\n", paddr, new_vaddr);
    word_t new_paddr = new_vaddr - get_vcpu().get_wedge_vaddr() + get_vcpu().get_wedge_paddr();
    map_info.addr = paddr + link_addr;
    map_info.page_bits = PAGE_BITS;
    fp_recv = L4_FpageLog2( map_info.addr, map_info.page_bits );
    fp_req = L4_FpageLog2(new_paddr, map_info.page_bits );
    idl4_set_rcv_window( &ipc_env, fp_recv );
    IResourcemon_request_pages( L4_Pager(), fp_req.raw, 7, &fp, &ipc_env );
    return true;
}

bool acpi_t::is_acpi_table(word_t paddr)
{
    paddr &= PAGE_MASK;
    return (contains_device_mem(paddr, paddr + PAGE_SIZE, 0x3f) ||
	    contains_device_mem(paddr, paddr + PAGE_SIZE, 0x4f));
    
}

void acpi_t::init()
{
    remap_acpi_mem((word_t) 0x40E, (word_t &) misc);
    bios_ebda = (*(L4_Word16_t *) misc) * 16;
    unmap_acpi_mem(0x40E);
    
    if (!bios_ebda)
	bios_ebda = 0x9fc00;
	    
    dprintf(debug_acpi, "ACPI EBDA  @ %x (virtual %x)\n", bios_ebda, virtual_ebda);

    
    if ((physical_rsdp = acpi_rsdp_t::locate_phys((word_t &) rsdp)) == NULL)
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
    dprintf(debug_acpi, "ACPI RSDP  @ %x, remap @ %x\n", physical_rsdp, rsdp);
	
    physical_rsdt = rsdp->get_rsdt_phys();
    physical_xsdt = rsdp->get_xsdt_phys();
		
    if (physical_xsdt == NULL && physical_rsdt == NULL)
    {
	printf( "ACPI neither RSDT nor XSDT found\n"); 
	return;
    }


    if (physical_xsdt != NULL)
    {
	xsdt = remap_acpi_mem(physical_xsdt, xsdt);
	dprintf(debug_acpi, "ACPI XSDT  @ %x, remap @ %x\n", physical_xsdt, xsdt);
		
	//xsdt->dump(physical_xsdt, misc);
	madt = xsdt->remap_entry("APIC", physical_madt, physical_xsdt, madt);
	dprintf(debug_acpi, "ACPI MADT  @ %x, remap @ %x\n", physical_madt, madt);

	fadt = rsdt->remap_entry("FACP", physical_fadt, physical_rsdt, fadt);
	dprintf(debug_acpi, "ACPI FADT  @ %x, remap @ %x\n", physical_fadt, fadt);

    }
	    
    if (physical_rsdt != NULL)
    {
	
	rsdt = remap_acpi_mem(physical_rsdt, rsdt);
	dprintf(debug_acpi, "ACPI RSDT  @ %x, remap @ %x\n", physical_rsdt, rsdt);
	
	rsdt->dump(physical_rsdt, misc);

	if (!physical_madt)
	{
	    madt = rsdt->remap_entry("APIC", physical_madt, physical_rsdt, madt);
	    dprintf(debug_acpi, "ACPI MADT  @ %x, remap @ %x\n", physical_madt, madt);
	}
	
	if (!physical_fadt)
	{
	    fadt = rsdt->remap_entry("FACP", physical_fadt, physical_rsdt, fadt);
	    dprintf(debug_acpi, "ACPI FADT  @ %x, remap @ %x\n", physical_fadt, fadt);
	}
    }
	    
	
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

#if !defined(CONFIG_DEVICE_PASSTHRU)
    if (physical_fadt)
    {
	virtual_dsdt_phys = virtual_table_phys((word_t) AmlCode);
	dprintf(debug_acpi+1, "ACPI vDSDT %x\n", AmlCode);
	
	fadt->copy(virtual_fadt);
	virtual_fadt_phys = virtual_table_phys((word_t) virtual_fadt);
	virtual_fadt->set_dsdt_phys(virtual_dsdt_phys);	
	
	dprintf(debug_acpi+1, "ACPI created and patched vFADT %x\n", 
		virtual_fadt, virtual_fadt_phys);

    }
#endif
    
    /*
     * Build virtual ACPI tables from physical ACPI tables
     */
	    
    for (word_t vcpu=0; vcpu < vcpu_t::nr_vcpus; vcpu++)
    {
	virtual_rsdp_phys[vcpu] = virtual_table_phys((word_t) virtual_rsdp[vcpu]);
	virtual_rsdt_phys[vcpu] = virtual_table_phys((word_t) virtual_rsdt[vcpu]);
	virtual_xsdt_phys[vcpu] = virtual_table_phys((word_t) virtual_xsdt[vcpu]);
	virtual_madt_phys[vcpu] = virtual_table_phys((word_t) virtual_madt[vcpu]);
		
	if (physical_rsdt)
	{
		    
	    rsdt->copy(virtual_rsdt[vcpu]);	
	    
	    virtual_rsdt[vcpu]->init_virtual();
#if !defined(CONFIG_DEVICE_PASSTHRU)
	    virtual_rsdt[vcpu]->set_entry_phys(physical_fadt, virtual_fadt_phys);
#endif
	    virtual_rsdt[vcpu]->set_entry_phys(physical_madt, virtual_madt_phys[vcpu]);
		    
	    rsdp->copy(virtual_rsdp[vcpu]);
	    virtual_rsdp[vcpu]->init_virtual();
		    
	    virtual_rsdp[vcpu]->set_rsdt_phys(virtual_rsdt_phys[vcpu]);
		    
	    dprintf(debug_acpi+1, "ACPI created and patched vRSDT %x, vRSDP %x for VCPU %d\n", 
		    virtual_rsdt[vcpu], virtual_rsdp[vcpu], vcpu);

	}
		
	if (physical_xsdt)
	{
  
	    xsdt->copy(virtual_xsdt[vcpu]);
		    
	    virtual_xsdt[vcpu]->init_virtual();
	    virtual_xsdt[vcpu]->set_entry_phys(physical_madt, virtual_madt_phys[vcpu]);	
	    
		    
	    if (!physical_rsdt)
	    {
		    
		rsdp->copy(virtual_rsdp[vcpu]);
		virtual_rsdp[vcpu]->init_virtual();
	    }
		    
	    dprintf(debug_acpi+1, "ACPI created and patched vXSDT %x, vRSDP %x for VCPU %d\n", 
		    virtual_xsdt[vcpu], virtual_rsdp[vcpu], vcpu);
	    virtual_rsdp[vcpu]->set_xsdt_phys(virtual_xsdt_phys[vcpu]);
		    
	}

	
    }    
    dprintf(debug_acpi, "ACPI initialized\n");
	    
    if (physical_rsdp)
	unmap_acpi_mem(physical_rsdp);
    if (physical_xsdt)
	unmap_acpi_mem(physical_xsdt);
    if (physical_rsdt)
	unmap_acpi_mem(physical_rsdt);
    if (physical_madt)
	unmap_acpi_mem(physical_madt);
    if (physical_fadt)
	unmap_acpi_mem(physical_fadt);

}


#endif /* !__PLATFORM__PC99__ACPI_H__ */
