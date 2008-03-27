/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/ia32/intlogic.cc
 * Description:   Front-end support for the interrupt handling.
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
 ********************************************************************/

#include <templates.h>
#include INC_ARCH(cpu.h)
#include INC_ARCH(intlogic.h)
#include INC_WEDGE(vcpulocal.h)
#include <debug.h>
#include <console.h>
#include INC_WEDGE(backend.h)
#include <device/acpi.h>

intlogic_t intlogic;

bool intlogic_t::deliver_synchronous_irq()
{
    word_t vector, irq;

#if !defined(CONFIG_SMP_ONE_AS)
    printf( "Warning: deprecated logic: deliver_synchronous_irq()\n");
#endif
    
    bool saved_int_state = get_cpu().disable_interrupts();
    if( !saved_int_state ) {
	// Interrupts were disabled.
	return false;
    }
    
    if( !pending_vector(vector, irq) )
    {
	get_cpu().restore_interrupts( saved_int_state );
	return false;
    }

    dprintf(irq_dbg_level(irq), "INTLOGIC deliver irq %d\n", irq);
    exc_info_t  exc;
    exc.vector = vector;
    exc.int_state = saved_int_state;
    exc.err_valid = false;
    backend_sync_deliver_exception( exc, 0 );
    return true;
}

#if defined(CONFIG_DEVICE_APIC)
void intlogic_t::init_virtual_apics(word_t real_irq_sources, word_t num_vcpus)
{
    word_t dummy;
    
    virtual_apic_config_t vapic_config;
    
    for (word_t vcpu=0; vcpu < num_vcpus; vcpu++)
    {
	extern local_apic_t lapic;
	vapic_config.lapic[vcpu].id = get_vcpu(vcpu).cpu_id;
	vapic_config.lapic[vcpu].cpu_id = get_vcpu(vcpu).cpu_id;
	vapic_config.lapic[vcpu].address = (word_t) 
 	    backend_resolve_addr( (word_t) &lapic, dummy)->get_address();
	vapic_config.lapic[vcpu].flags.enabled = 1;
	vapic_config.lapic[vcpu].flags.configured = 1;
	
	dprintf(debug_irq, "INTLOGIC virtual LAPIC id %d addr %x (%x)\n",
		vapic_config.lapic[vcpu].id, vapic_config.lapic[vcpu].address, &lapic);
	ASSERT(vapic_config.lapic[vcpu].address);

    }

    /*
     * We poke in the following APIC configuration
     * 
     * IOAPIC0: GSI  0 .. 23
     * IOAPCI1: GSI 24 .. 47
     * ...
     * 
     */
    const word_t gsi_per_ioapic = 24;
    word_t irq_sources = real_irq_sources + virtual_irq_sources ;
    word_t nr_ioapics = irq_sources / gsi_per_ioapic + (irq_sources % gsi_per_ioapic != 0);
    word_t start_id = num_vcpus + 1;
    
    dprintf(debug_irq, "INTLOGIC found %d physical interrupts, adding %d virtual interrupts\n",
	    real_irq_sources, virtual_irq_sources);
    
    if (nr_ioapics >= CONFIG_MAX_IOAPICS)
    {
	dprintf(debug_irq, "INTLOGIC not enough  virtual APICs for %d GSIs\n", real_irq_sources);
	panic(); 
    }
    
    for (word_t ioapic=0; ioapic < nr_ioapics; ioapic++)
    {
	word_t gsi_min = gsi_per_ioapic * ioapic;
	word_t gsi_max = min (gsi_min + gsi_per_ioapic, irq_sources) - 1; 

	vapic_config.ioapic[ioapic].id = start_id + ioapic;
	vapic_config.ioapic[ioapic].address = 
	    (word_t) backend_resolve_addr((word_t) &intlogic.virtual_ioapic[ioapic], dummy)->get_address();
	vapic_config.ioapic[ioapic].irq_base = gsi_per_ioapic * ioapic;
	vapic_config.ioapic[ioapic].max_redir_entry = gsi_max - gsi_min;
	vapic_config.ioapic[ioapic].flags.configured = 1;
	
	intlogic.virtual_ioapic[ioapic].set_id(start_id + ioapic);
	intlogic.virtual_ioapic[ioapic].set_base(gsi_per_ioapic * ioapic);
	intlogic.virtual_ioapic[ioapic].set_max_redir_entry(gsi_max - gsi_min);
	
	
	printf("INTLOGIC virtual IOAPIC id %d\n\t\t address %x (%x) \n\t\t  irq_base %x "
		"\n\t\t max_redir_entry %x\n\t\tregistering GSI %d-%d\n",
		vapic_config.ioapic[ioapic].id, vapic_config.ioapic[ioapic].address,
		&intlogic.virtual_ioapic[ioapic], vapic_config.ioapic[ioapic].irq_base,
		vapic_config.ioapic[ioapic].max_redir_entry, gsi_min, gsi_max);
	
	for (word_t gsi = gsi_min; gsi <= gsi_max;  gsi++)
	{
	    intlogic.hwirq_register_ioapic(gsi, &intlogic.virtual_ioapic[ioapic]);
	}
 

    }	
	
    // mark all remaining IO-APICs as invalid
    for (word_t apic = nr_ioapics; apic < CONFIG_MAX_IOAPICS; apic++)
    {
	intlogic.virtual_ioapic[apic].set_id(0xf);
	vapic_config.ioapic[apic].id = 0xf;
	vapic_config.ioapic[apic].flags.configured = 0;
    }	
    
    acpi.init_virtual_madt(vapic_config, num_vcpus); 
    printf( "INTLOGIC initialized\n");

}
    
#endif /* defined(CONFIG_DEVICE_APIC) */
