/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/main.cc
 * Description:   
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
 * $Id: irq.cc,v 1.1 2006/01/11 18:33:15 stoess Exp $
 *
 ********************************************************************/

#if defined(CONFIG_DEVICE_APIC)
#include <device/acpi.h>
#include <device/apic.h>
local_apic_t __attribute__((aligned(4096))) lapic;
acpi_t acpi;

extern word_t tmp_region[];

/**
 * Since we give passthrough access to ACPI, we have to make the virtual IO-APIC
 * configuration identical to the physical. 
 * 
 */

static word_t ioapic_get_max_redir_entries(u32_t ioapic_phys)
{
    word_t ioapic = (word_t) tmp_region;
    
    xen_memory.map_device_memory(ioapic, ioapic_phys, true);
    *(__volatile__ u32_t*) (ioapic + i82093_t::IOAPIC_SEL) = i82093_t::IOAPIC_VER;
    i82093_version_reg_t reg =  { raw : *(__volatile__ u32_t*) (ioapic + i82093_t::IOAPIC_WIN) };
    return reg.x.mre;
    xen_memory.unmap_device_memory(ioapic, ioapic_phys, true);
}    

bool init_io_apics()
{
    int apic;
    int nr_ioapics = acpi.get_nr_ioapics();
    intlogic_t &intlogic = get_intlogic();

    if (!nr_ioapics) {
	con << "IOAPIC: Initialization of APICs not possible, ignore...\n";
	DEBUGGER_ENTER(0);
	return false;
    }
    if (nr_ioapics >= CONFIG_MAX_IOAPICS)
    {
	con << "IOAPIC: more real IOAPICs than virtual APICs\n";
	panic();
    }

    con << "IOAPIC found " << nr_ioapics << " apics\n";

    for (apic=0; apic < nr_ioapics; apic++)
    {
	intlogic.ioapic[apic].set_id(acpi.get_ioapic_id(apic));
	intlogic.ioapic[apic].set_base(acpi.get_ioapic_irq_base(apic));
	intlogic.ioapic[apic].set_max_redir_entry(
	    ioapic_get_max_redir_entries(acpi.get_ioapic_addr(apic)));	
	
	con << "IOAPIC id " << acpi.get_ioapic_id(apic)
	    << ",  " << intlogic.ioapic[apic].get_max_redir_entry() << "\n";

    }
    
    // mark all remaining IO-APICs as invalid
    for (apic = nr_ioapics; apic < CONFIG_MAX_IOAPICS; apic++)
	intlogic.ioapic[apic].set_id(0xf);

    // pin/hwirq to IO-APIC association
    for (word_t i = 0; i < CONFIG_MAX_IOAPICS; i++)
    {
	if (!intlogic.ioapic[i].is_valid_ioapic())
	    continue;

	word_t hwirq_min = intlogic.ioapic[i].get_base();
	word_t hwirq_max = hwirq_min + intlogic.ioapic[i].get_max_redir_entry() + 1;
	hwirq_max = hwirq_max >= INTLOGIC_MAX_HWIRQS ? INTLOGIC_MAX_HWIRQS: hwirq_max;
	    
	for (word_t hwirq = hwirq_min; hwirq < hwirq_max; hwirq++)
	{
	    intlogic.hwirq_register_ioapic(hwirq, &intlogic.ioapic[i]);
	    con << "IOAPIC registering hwirq " << hwirq 
		<< " with apic " << intlogic.ioapic[i].get_id() << "\n";
	}
    }
	
    return true;
}
    

#endif


