/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/device/i82093
 * Description:   Front-end emulation for the IO APIC 82093.
 *
 * Re istribution and use in source and binary forms, with or without
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
 * $Id: i82093.cc,v 1.3 2006/01/11 17:41:46 store_mrs Exp $
 *
 ********************************************************************/

#include <device/portio.h>
#include INC_ARCH(intlogic.h)
#include INC_ARCH(bitops.h)
#include <console.h>
#include <debug.h>
#include INC_WEDGE(backend.h)

#ifdef CONFIG_WEDGE_XEN
#include INC_WEDGE(hypervisor.h)
#endif

const char ioapic_virt_magic[8] = { 'V', 'I', 'O', 'A', 'P', 'I', 'C', '0' };
const char *ioapic_delmode[8] = { "fixed", "lowest priority", "SMI", "reserved",
				  "NMI", "INT", "reserved", "extINT" };


extern "C" void __attribute__((regparm(2)))
    ioapic_write_patch( word_t value, word_t addr )
{
    i82093_t *ioapic = i82093_t::addr_to_ioapic(addr);
    UNUSED word_t dummy;
    
    if(debug_apic_sanity)
    {
	word_t paddr = 0;
	pgent_t *pgent = backend_resolve_addr(addr, dummy);
	
	if( !pgent || !pgent->is_kernel() )
	{
	    printf( "Inconsistent guest pagetable entries for IO-APIC mapping @ %x pgent %x\n",
		    addr, pgent);
	    panic();
	}

	paddr = pgent->get_address() + (addr & ~PAGE_MASK);

	if (!ioapic->is_valid_virtual_ioapic()) 
	    printf( "BUG no sane softIOAPIC @ %x phys %x pgent %x\n",
		     ioapic, pgent->get_address(), pgent);
	
    }	
    
    dprintf(debug_apic, "IOAPIC %d write addr %x (%x) value %x IP%x\n",
	    ioapic->get_id(), addr, backend_resolve_addr(addr, dummy)->get_address(), 
	    value,  __builtin_return_address(0));
    	
    ASSERT (ioapic->is_valid_virtual_ioapic());
    ioapic->write(value, ioapic->addr_to_reg(addr));
   
}

extern "C" word_t __attribute__((regparm(1)))
ioapic_read_patch( word_t addr )
{
    UNUSED word_t dummy;
    
    if( debug_apic )
    {
	i82093_t *ioapic = (i82093_t *) (addr & PAGE_MASK);
	word_t paddr = 0;
	pgent_t *pgent = backend_resolve_addr(addr, dummy);
	
	if( !pgent || !pgent->is_kernel() )
	{
	    printf( "Inconsistent guest pagetable entries for IO-APIC mapping @ %x pgent %x\n",
		    addr, pgent);
	    panic();
	}

	paddr = pgent->get_address() + (addr & ~PAGE_MASK);

	if (addr != (word_t) &ioapic->fields.mm_regs.regwin)
	    printf("IOAPIC strange read on non-regwin addr %x (%x) IP%x \n", 
		   addr, paddr,  __builtin_return_address(0));
	
	
	if (!ioapic->is_valid_virtual_ioapic())
	    PANIC( "BUG no sane softIOAPIC @ %x phys %x pgent %x\n",
		   ioapic, pgent->get_address(), pgent);
    }
    
    dprintf(debug_apic, "IOAPIC %d write addr %x (%x) value %x IP %x\n",
	    (i82093_t *) (addr & PAGE_MASK), addr, 
	    backend_resolve_addr(addr, dummy)->get_address(),
	    * (word_t *)addr,  __builtin_return_address(0));

    ASSERT(i82093_t::addr_to_ioapic(addr)->is_valid_virtual_ioapic());
    return * (word_t *) addr;

}

void i82093_t::eoi(word_t hwirq)
{
    word_t entry = hwirq - fields.irq_base;
    ASSERT(entry < IOAPIC_MAX_REDIR_ENTRIES);

    // always clear IRR (don't check for edge/level)
    if (bit_test_and_clear_atomic(14, fields.io_regs.x.redtbl[entry].raw[0]))
    {
	intlogic_t &intlogic = get_intlogic();
	if (!intlogic.is_hwirq_squashed(hwirq) &&
	    intlogic.test_and_clear_hwirq_mask(hwirq))
	{
	    dprintf(irq_dbg_level(hwirq)+1, "IOAPIC %d EOI irq %d unmask\n", get_id(), hwirq);
	    
	    cpu_t &cpu = get_cpu();
	    word_t int_save = cpu.disable_interrupts();
	    ASSERT(get_redir_entry_dest_mask(entry));
	    backend_unmask_device_interrupt(hwirq);
	    cpu.restore_interrupts( int_save );

	}
	
	if (bit_test_and_clear_atomic(17, fields.io_regs.x.redtbl[entry].raw[0]))
	{
	    printf("IOAPIC %d UNTESTED rereaise of pending irq %d\n", get_id(), hwirq);
	    DEBUGGER_ENTER("IOAPIC");
	    raise_irq(hwirq, true);
	}
    }

}

void i82093_t::enable_redir_entry_hwirq(word_t entry)
{
    const word_t hwirq = entry + fields.irq_base;
    word_t new_dest_id_mask = get_redir_entry_dest_mask(entry);
    word_t new_dest_id = (new_dest_id_mask) ? lsb(new_dest_id_mask) : CONFIG_NR_VCPUS;
     
    if (get_intlogic().test_and_set_hwirq_latch(hwirq))
    {
	word_t old_dest_id_mask = get_redir_entry_dest_mask(entry, true);
	word_t old_dest_id = (old_dest_id_mask) ? lsb(old_dest_id_mask) : CONFIG_NR_VCPUS; 
	
	if (old_dest_id != new_dest_id)
	{
	    dprintf(irq_dbg_level(hwirq), "IOAPIC %d IRQ migration irq %d VCPU %d -> %d\n",
		    hwirq, old_dest_id, new_dest_id);
	    /*
	     * Migration 
	     */
	    if (old_dest_id != CONFIG_NR_VCPUS)
	    {
		vcpu_t &old_dest_vcpu = get_vcpu(old_dest_id);
		
		dprintf(irq_dbg_level(hwirq), "IOAPIC %d IRQ disable irq %d on VCPU %d\n",
			hwirq, old_dest_id);

		if( !backend_disable_device_interrupt(hwirq, old_dest_vcpu) )
		    printf("IOAPIC %d IRQ unable to disable irq %d on VCPU %d during migration\n",
			    hwirq, old_dest_id);
	    }
	    
	    if (new_dest_id != CONFIG_NR_VCPUS)
	    {
		vcpu_t &new_dest_vcpu = get_vcpu(new_dest_id);	
		
		dprintf(irq_dbg_level(hwirq), "IOAPIC %d IRQ enable irq on VCPU %d\n",
			hwirq, new_dest_id);
		
		if( !backend_enable_device_interrupt(hwirq, new_dest_vcpu) )
		    printf("IOAPIC %d IRQ unable to enable irq %d on VCPU %d during migration\n",
			    hwirq, new_dest_id);
	    }

	}
		
    }
    else if (new_dest_id != CONFIG_NR_VCPUS)
    {
	/*
	 * Initial enable
	 */
	vcpu_t &new_dest_vcpu = get_vcpu(new_dest_id);	

	dprintf(irq_dbg_level(hwirq), "IOAPIC %d IRQ enable irq on VCPU %d\n",
		hwirq, new_dest_id);

	
	if( !backend_enable_device_interrupt(hwirq, new_dest_vcpu) )
	    printf("IOAPIC %d IRQ unable to enable irq %d on VCPU %d\n",
			   hwirq, new_dest_id);
    }
    
    
    fields.io_regs.x.redtbl[entry].x.old_dstm = 
	fields.io_regs.x.redtbl[entry].x.dstm;
    
    switch (fields.io_regs.x.redtbl[entry].x.dstm)
    {
	case IOAPIC_DS_PHYSICAL:
	    fields.io_regs.x.redtbl[entry].x.dest.phys.old_pdst = 
		fields.io_regs.x.redtbl[entry].x.dest.phys.pdst;
	    break;
		
	case IOAPIC_DS_LOGICAL:
	    fields.io_regs.x.redtbl[entry].x.dest.log.old_ldst = 
		    fields.io_regs.x.redtbl[entry].x.dest.log.ldst;
		
	    break;
	default:
	    printf( "IOAPIC %d invalid destination RDTBL %d value %x\n", 
		     get_id(), entry, fields.io_regs.x.redtbl[entry].raw);
	    DEBUGGER_ENTER("IOAPIC");
	    break;
    }
    
}




void i82093_t::raise_irq (word_t irq, bool reraise)
{
    word_t entry = irq - fields.irq_base;
    ASSERT(entry < IOAPIC_MAX_REDIR_ENTRIES);
    
    intlogic_t &intlogic = get_intlogic();
    vcpu_t &vcpu = get_vcpu();
    
    dprintf(irq_dbg_level(irq)+1, "IOAPIC %d irq %d entry %d fields %x/%x\n",
	    irq, entry, (word_t) fields.io_regs.x.redtbl[entry].raw[1],
	    fields.io_regs.x.redtbl[entry].raw[0]);
    
    if (fields.io_regs.x.redtbl[entry].x.vec <= 15)
	return;

    intlogic.set_hwirq_mask(irq);

    if (fields.io_regs.x.redtbl[entry].x.msk == 0)
    {
	/*
	 * Get GSI destination
	 * LAPIC physical IDs are identical to VCPU IDs
	 */

	word_t dest_id_mask = get_redir_entry_dest_mask(entry);	
	word_t vector = fields.io_regs.x.redtbl[entry].x.vec;
	bool from_eoi = false;
	i82093_t *from = NULL;
	
	if (fields.io_regs.x.redtbl[entry].x.trg == IOAPIC_TR_LEVEL) 
	{
	    // if level triggered, set IRR 
	    bit_set_atomic(14, fields.io_regs.x.redtbl[entry].raw[0]);
	    // Only fixed (000) and lowest priority (001) IRQs need eoi 
	    from_eoi = (fields.io_regs.x.redtbl[entry].x.del <= 1);
	    from = this;
	}
	else
	{
	    if (!intlogic.is_hwirq_squashed(irq) &&
		    intlogic.test_and_clear_hwirq_mask(irq))
	    {
		dprintf(irq_dbg_level(irq)+1, "Unmask edge triggered IRQ %d\n", irq);

		cpu_t &cpu = get_cpu();
		word_t int_save = cpu.disable_interrupts();
		ASSERT(get_redir_entry_dest_mask(entry));
		backend_unmask_device_interrupt(irq);
		cpu.restore_interrupts( int_save );
	    }
	}
	
	while (dest_id_mask)
	{
	    word_t dest_id = lsb(dest_id_mask);

	    dprintf(irq_dbg_level(irq)+1, "IOAPIC %d send IRQ %d to APIC %d to mask %x vector %d current VCPU %d\n",
		    get_id(), irq, dest_id, dest_id_mask, vector, vcpu.cpu_id);
	    
	    if (dest_id >= CONFIG_NR_VCPUS)
		printf("IOAPIC %d send IRQ %d to APIC %d to mask %x vector %d current VCPU %d\n",
			get_id(), irq, dest_id, dest_id_mask, vector, vcpu.cpu_id);
	    
	    dest_id_mask &= ~(1 << dest_id);
	    if (dest_id >= CONFIG_NR_VCPUS)
		DEBUGGER_ENTER("IOAPIC");
	    local_apic_t &remote_lapic = get_lapic(dest_id);
	    remote_lapic.lock();
	    remote_lapic.raise_vector(vector, irq, reraise, from_eoi, from);
	    remote_lapic.unlock();

	}
    }
    // If masked level irq mark pending, abuse bit 17 for this purpose
    else if (fields.io_regs.x.redtbl[entry].x.trg == 1) 
    {
	dprintf(irq_dbg_level(irq), "IOAPIC %d mark masked level irq %d pending\n", get_id(), irq);
	bit_set_atomic(17, fields.io_regs.x.redtbl[entry].raw[0]);
    }
    else 
    {
	dprintf(irq_dbg_level(irq), "IOAPIC %d ack and ignore masked edge irq %d\n", get_id(), irq);
	if (!intlogic.is_hwirq_squashed(irq) &&
		intlogic.test_and_clear_hwirq_mask(irq))
	{
	    dprintf(irq_dbg_level(irq), "IOAPIC %d ack and unmask masked edge irq %d\n", get_id(), irq);
	    cpu_t &cpu = get_cpu();
	    word_t int_save = cpu.disable_interrupts();
	    ASSERT(get_redir_entry_dest_mask(entry));
	    backend_unmask_device_interrupt(irq);
	    cpu.restore_interrupts( int_save );
	}
    }
}	    


void i82093_t::write(word_t value, word_t reg)
{
    switch (reg) 
    {
	case IOAPIC_SEL:
	{
	    dprintf(debug_apic, "IOAPIC %d register select %x\n", get_id(), value);
	    fields.mm_regs.regsel = value;
		    
	    /*
	     * js: is prefetching really preferable ?
	     */
	    if (value < 16 + (2 * IOAPIC_MAX_REDIR_ENTRIES))
		fields.mm_regs.regwin = fields.io_regs.raw[value];

	    else
		printf( "BUG: strange io regster selection for IOAPIC (%x)\n", value);
	    break;
	}
	case IOAPIC_WIN:
	{
	    dprintf(debug_apic, "IOAPIC %d write %x to reg %x\n",  get_id(), value, fields.mm_regs.regsel);
	    
	    switch (fields.mm_regs.regsel) 
	    {
		case IOAPIC_ID:	
		{		    
		    i82093_id_reg_t nid = { raw : value };
		    
		    dprintf(debug_apic, "IOAPIC %d set ID to %x\n", get_id(), nid.x.id);
		    
		    fields.io_regs.x.id.x.id = nid.x.id;
		    break;
		}
		case IOAPIC_VER:
		case IOAPIC_ARB:
		{
		    printf("IOAPIC %d write ID to %x reg %x UNIMPLEMENTED\n", get_id(), value, fields.mm_regs.regsel);
		    DEBUGGER_ENTER("IOAPIC");
		}
	    
		case IOAPIC_RDTBL_0_0 ... IOAPIC_RDTBL_23_1:
		{
		    const word_t entry = (fields.mm_regs.regsel - IOAPIC_RDTBL_0_0) >> 1;
		    //bool int_save;		
		    cpu_t cpu = get_cpu();
		    i82093_redtbl_t nredtbl;
		    const word_t hwirq = entry + fields.irq_base;
			
		    /* 
		     * Lower half of register ?
		     */
		    if ((fields.mm_regs.regsel & 1) == 0) 
		    {
				
			word_t save_msk = fields.io_regs.x.redtbl[entry].x.msk;
			nredtbl.raw[0]  = value;			
			
			//int_save = cpu.disable_interrupts();
			
			fields.io_regs.x.redtbl[entry].x.dstm  = nredtbl.x.dstm;
			fields.io_regs.x.redtbl[entry].x.msk  = nredtbl.x.msk;
			fields.io_regs.x.redtbl[entry].x.trg  = nredtbl.x.trg;
			fields.io_regs.x.redtbl[entry].x.pol  = nredtbl.x.pol;
			fields.io_regs.x.redtbl[entry].x.dstm = nredtbl.x.dstm;
			fields.io_regs.x.redtbl[entry].x.del  = nredtbl.x.del;
			fields.io_regs.x.redtbl[entry].x.vec  = nredtbl.x.vec;
			    
			//cpu.restore_interrupts( int_save );

			dprintf(irq_dbg_level(hwirq), 
				"IOAPIC %d set RDTBL %d addr %x %s %s triggered %s"
				"active %s destination mode %s delivery mode vector %d\n", 
				get_id(), entry, &fields.io_regs.x.redtbl[entry],
				((nredtbl.x.msk == 1) ? "masked" : "unmasked"),
				((nredtbl.x.trg == 1) ? "level" : "edge"),
				((nredtbl.x.pol == 1) ? "low" : "high"), 
				((nredtbl.x.dstm == 1) ? "logical" : "physical"),
				ioapic_delmode[nredtbl.x.del], nredtbl.x.vec);
			
			if (nredtbl.x.msk == 0 && save_msk == 1)
			{
			    dprintf(irq_dbg_level(hwirq), "IOAPIC %d unmask irq %d\n", get_id(), hwirq);
			    enable_redir_entry_hwirq(entry);
			    
			    if (bit_test_and_clear_atomic(17, fields.io_regs.x.redtbl[entry].raw[0]))
			    {
				printf("IOAPIC %d reraise pending irq %d untested\n", 
					get_id(), hwirq);
				DEBUGGER_ENTER("IOAPIC");
				raise_irq(hwirq, true);
			    }
			}
			else if (nredtbl.x.msk == 1 && save_msk == 0)
			{	
			    dprintf(irq_dbg_level(hwirq), "IOAPIC %d unmask irq %d\n", get_id(), hwirq);
			}
		    }
		    else {
			 
			nredtbl.raw[1]  = value;			

			//int_save = cpu.disable_interrupts();
			
			switch (fields.io_regs.x.redtbl[entry].x.dstm)
			{
			    case IOAPIC_DS_PHYSICAL:
				fields.io_regs.x.redtbl[entry].x.dest.phys.pdst = 
				    nredtbl.x.dest.phys.pdst;
				break;
			    case IOAPIC_DS_LOGICAL:
				fields.io_regs.x.redtbl[entry].x.dest.log.ldst = 
				    nredtbl.x.dest.log.ldst;
				break;
			    default:
			    {
				printf( "IOAPIC %d invalid destination RDTBL %d value %x\n", 
					get_id(), entry, fields.io_regs.x.redtbl[entry].raw);
				DEBUGGER_ENTER("IOAPIC");
				break;
			    }
			}
			dprintf(irq_dbg_level(hwirq), "IOAPIC %d set RDTBL %d %s destination to %d (prev %s %d)\n",
				get_id(), entry, ((fields.io_regs.x.redtbl[entry].x.dstm == 1) ? " logical" : " physical"),
				nredtbl.x.dest.log.ldst, 
				((fields.io_regs.x.redtbl[entry].x.old_dstm == 1) ? " logical" : " physical"), 
				nredtbl.x.dest.log.old_ldst);
			
			if (fields.io_regs.x.redtbl[entry].x.msk == 0)
			    enable_redir_entry_hwirq(entry);
			
			//cpu.restore_interrupts( int_save );

		    }
		    break;		   
		}
		default:
		{
		    printf( "IOAPIC %d write to non-emulated register %x value %x\n", get_id(), fields.mm_regs.regsel, value);
		    DEBUGGER_ENTER("IOAPIC");
		    break;
		}
	    }
	    break;
	}
	default:
	{
	    printf( "IOAPIC %d write to non-window reg %x value %x\n", get_id(), reg, value);
	    DEBUGGER_ENTER("IOAPIC");
	    break;
	}
    }
}

