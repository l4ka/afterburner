/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/device/i82093.h
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
 * $Id: lapic.cc,v 1.3 2006/01/11 17:42:18 store_mrs Exp $
 *
 ********************************************************************/
#include INC_ARCH(bitops.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(debug.h)
#include <device/lapic.h>
#include <bind.h>

const char *lapic_delmode[8] =
{ "fixed", "reserved", "SMI", "reserved", 
  "NMI", "INIT", "reserved", "ExtINT" };

extern "C" void __attribute__((regparm(2))) lapic_write_patch( word_t value, word_t addr )
{
    local_apic_t &lapic = get_lapic();
    ASSERT(lapic.get_id() == get_vcpu().cpu_id);

    if( debug_lapic )
    {
	word_t dummy, paddr = 0;
	pgent_t *pgent = backend_resolve_addr(addr, dummy);
	
	if( !pgent || !pgent->is_kernel() )
	{
	    con << "Inconsistent guest pagetable entries for local APIC mapping @ "
		<< (void *) addr << ", pgent " << (void *) pgent << "\n"; 
	    panic();
	}

	
	if (!lapic.is_valid_lapic())
	    PANIC( "BUG no sane softLAPIC @%x, phys %x, pgent %x\n",
		   &lapic, pgent->get_address(), pgent); 
	
	paddr = pgent->get_address() + (addr & ~PAGE_MASK);
	
	if (debug_lapic_reg)
	    con << "LAPIC " << lapic.get_id() 
		<< " write " << (void *) &lapic 
		<< " reg " << (void *) lapic.addr_to_reg(addr)
		<< " (" << (void *)addr 
		<< "/"  << (void *) paddr 
		<< "), ip " <<  __builtin_return_address(0) 
		<< ", value  " << (void *) value << "\n" ;
	
    }
    lapic.write(value, lapic.addr_to_reg(addr));
}

extern "C" word_t __attribute__((regparm(1))) lapic_read_patch( word_t addr )
{
    local_apic_t &lapic = get_lapic();
    ASSERT(lapic.get_id() == get_vcpu().cpu_id);

    if (lapic.addr_to_reg(addr) == local_apic_t::LAPIC_REG_TIMER_COUNT)
    {
	con << "LAPIC reading TIMER COUNT UNIMPLEMENTED -- must be computed!\n";
	//DEBUGGER_ENTER(0);
    }
    
    lapic.lock();
    word_t ret = lapic.get_reg(lapic.addr_to_reg(addr));
    lapic.unlock();
    
    if( debug_lapic )
    {
	word_t dummy, paddr = 0;
	pgent_t *pgent = backend_resolve_addr(addr, dummy);
	
	if( !pgent || !pgent->is_kernel() )
	{
	    con << "Inconsistent guest pagetable entries for local APIC mapping @" 
		<< (void *) addr << ", pgent " << (void *) pgent << "\n"; 
	    panic();
	}

	if (!lapic.is_valid_lapic())
	    PANIC( "BUG no sane softLAPIC @%x, phys %x, pgent %x\n",
		   &lapic, pgent->get_address(), pgent); 
	
	
	paddr = pgent->get_address() + (addr & ~PAGE_MASK);
        
	if (debug_lapic_reg)
	{
	    con << "LAPIC " << lapic.get_id() 
		<< " read  " << (void *) &lapic 
		<< " reg " << (void *) lapic.addr_to_reg(addr)
		<< " (" << (void *)addr 
		<< "/"  << (void *) paddr 
		<< "), ip " <<  __builtin_return_address(0) 
		<< ", ret  " << (void *) ret << "\n" ;
	}
	
    }
    
    return ret;

}

extern "C" word_t __attribute__((regparm(2))) lapic_xchg_patch( word_t value, word_t addr )
{
    // TODO: confirm that the address maps to our bus address.  And determine
    // the  device from the address.
    local_apic_t &lapic = get_lapic();
    ASSERT(lapic.get_id() == get_vcpu().cpu_id);
    
    con << "LAPIC " << lapic.get_id() << " UNTESTED xchg @ " << (void *)addr 
	<< ", value " << (void *)value 
	<< ", ip " << __builtin_return_address(0) << '\n';

    lapic.lock();
    word_t ret = lapic.get_reg(lapic.addr_to_reg(addr));
    lapic.write(value, lapic.addr_to_reg(addr));
    lapic.unlock();
    DEBUGGER_ENTER(0);
    return ret;

}

bool local_apic_t::pending_vector( word_t &vector, word_t &irq)
{
    intlogic_t &intlogic = get_intlogic();
    ASSERT(get_vcpu().cpu_id == get_id());

    /* 
     * Find highest bit in IRR and ISR
     */
    
    vector = msb_rr(lapic_rr_irr);
    
    word_t vector_in_service = msb_rr(lapic_rr_isr);
    word_t ret = false;
    
    if ((vector >> 4) > (vector_in_service >> 4) && vector > 15)
    {    
	/*
	 * Shift to ISR
	 */
	bit_clear_atomic_rr(vector, lapic_rr_irr);
	bit_set_atomic_rr(vector, lapic_rr_isr);
	irq = get_pin(vector);
	
	if(intlogic.is_irq_traced(irq, vector))
	{
	    i82093_t *from = get_ioapic(vector);
	    con << "LAPIC " << get_id() << " found pending vector in IRR " << vector
		<< " ISR " << vector_in_service ;
	    if (from) 
		con << " IRQ " << irq
		    << " IO-APIC " << from->get_id();
	    con << "\n";
	}
	ret = true;
    }
    else 
	ret = false;
    return ret;

}

void local_apic_t::raise_vector(word_t vector, word_t irq, bool reraise, bool from_eoi, i82093_t *from )
{
    ASSERT(vector <= APIC_MAX_VECTORS);
    
    if (vector > 15)
    {

	if (reraise)
	{
	    // clear ISR bit raised in previous pending_vector
	    bit_clear_atomic_rr(vector, lapic_rr_isr);
	}
	
	bit_set_atomic_rr(vector, lapic_rr_irr);
	set_vector_cluster(vector);

	if (from_eoi)
	{
	    set_ioapic(vector, from);
	    bit_set_atomic_rr(vector, lapic_rr_tmr);
	}
	set_pin(vector, irq);

	if(get_intlogic().is_irq_traced(irq, vector))
	{
	    con << "LAPIC " << get_id() << " raise vector " << vector 
		<< " IRQ " << irq 
		<< " from IOAPIC " << (from ? from->get_id(): 0) 
		<< " from_eoi " << from_eoi
		<< " reraise " << reraise
		<< "\n";
	}

    }
}


void local_apic_t::write(word_t value, word_t reg)
{
    ASSERT(get_vcpu().cpu_id == get_id());
	    
    switch (reg) {
	
	case LAPIC_REG_VER:
	{
	    if (debug_lapic)
		con << "LAPIC " << get_id() << " write to r/o version register " << (void*) value << "\n";
	}
	break;
	case LAPIC_REG_LDR:
	{
	    lapic_id_reg_t nid = { raw: value };
	    if(debug_lapic) 
		con << "LAPIC " << get_id() << " set logical destination to " << (void*) nid.x.id << "\n";
	    fields.ldest.x.id = nid.x.id;
	}
	break;
	case LAPIC_REG_DFR:
	{
	    lapic_dfr_reg_t ndfr = { raw : value};
	    fields.dfr.x.model = ndfr.x.model;
	    
	    /* we only support the flat format */
	    switch (fields.dfr.x.model)
	    {
		case LAPIC_DF_FLAT:
		{
		    if(debug_lapic) 
			con << "LAPIC " << get_id() 
			    << " set DFR to flat model\n";
		}
		break;
		case LAPIC_DF_CLUSTER:
		{
		    con << "LAPIC " << get_id() 
			<< " UNIMPLEMENTED dfr cluster format " << ndfr.x.model << "\n";
		    DEBUGGER_ENTER(0);
		}
		break;
		default:
		{
		    con << "LAPIC " << get_id() 
			<< " INVALID dfr format " << ndfr.x.model << "\n";
		    DEBUGGER_ENTER(0);
		}
		break;
	    }
	}
	break;
	case LAPIC_REG_TPR:
	{
	    lapic_prio_reg_t ntpr = { raw: value };	

	    if (ntpr.x.prio == 0 && ntpr.x.subprio == 0)
	    {
		if (debug_lapic) 
		    con << "LAPIC " << get_id() 
			<< " set task prio to accept all ";
		return;
	    }
	    else
	    {
		if (debug_lapic) 
		    con << "LAPIC " << get_id() 
			<< " unsupported task priorities " 
			<< ntpr.x.prio << "/" << ntpr.x.subprio 
			<< "\n";
		DEBUGGER_ENTER(0);
	    }
	}
	break;
	case LAPIC_REG_EOI:
	{
	    intlogic_t &intlogic = get_intlogic();
	    
	    word_t vector = msb_rr( lapic_rr_isr );
	    if( vector > 15 && bit_test_and_clear_atomic_rr(vector, lapic_rr_isr))
	    {
		
		if (msb_rr_cluster(lapic_rr_isr, vector) == 0)
		    clear_vector_cluster(vector);

		word_t irq = get_pin(vector);

		if (bit_test_and_clear_atomic_rr(vector, lapic_rr_tmr) &&
			get_ioapic(vector))
		{
		    ASSERT(get_ioapic(vector)->is_valid_ioapic());
		    ASSERT(irq < INTLOGIC_MAX_HWIRQS);
		    get_ioapic(vector)->eoi(irq);
		}
		
		if(intlogic.is_irq_traced(irq))
		{
		    con << "LAPIC " << get_id() << " EOI vector " << vector;
		    if (get_ioapic(vector))
			con << " IRQ " << irq
			    << " IO-APIC " << get_ioapic(vector)->get_id();
		    con << "\n";
		}
		
		
	    }
	}		    
	break;
	case LAPIC_REG_SVR:
	{
	    lapic_svr_reg_t nsvr = { raw : value };

	    fields.svr.x.vector = nsvr.x.vector;
	    fields.svr.x.focus_processor = nsvr.x.focus_processor;
		
	    if(debug_lapic)
	    {
		con << "LAPIC " << get_id() << " set spurious vector to " << fields.svr.x.vector << "\n";
		con << "LAPIC " << get_id() << " set focus processor to " << fields.svr.x.focus_processor << "\n";
	    }
		
	    if ((fields.svr.x.enabled = nsvr.x.enabled))
	    {
		if (debug_lapic)
		    con << "LAPIC " << get_id() << " enabled\n";
		fields.enabled = true;
#if defined(CONFIG_DEVICE_PASSTHRU)
		/* 
		 * jsXXX: for passthrough PIT 8253 and IRQ0 override
		 * uncomment the following line (see device/portio.cc)
		 */
		//get_intlogic().clear_hwirq_squash(2);
#endif
	    }
	    else
	    {
		con << "LAPIC " << get_id() << " disable unimplemented\n";
		DEBUGGER_ENTER(0);
	    }
	}
	break;
	case LAPIC_REG_LVT_TIMER:
	{ 
	    lapic_lvt_timer_t ntimer = { raw : value};
	    
	    fields.timer.x.vec = ntimer.x.vec;
	    /* Only support periodic timers for now */
	    fields.timer.x.msk = ntimer.x.msk;
	    fields.timer.x.mod = ntimer.x.mod;

	    if(debug_lapic) 
		con << "LAPIC " << get_id() << " set TIMER\n\t\t"
		    << ((ntimer.x.msk == 1) ? "masked" : "unmasked") << "\n\t\t"
		    << " vector to " << ntimer.x.vec << "\n";
	    if (ntimer.x.mod == 0 && ntimer.x.msk == 0)
	    {
		con << "LAPIC " << get_id() << " unsupported timer mode (one-shot)\n";
		DEBUGGER_ENTER(0);
	    }
	    
	}
	break;
	case LAPIC_REG_LVT_LINT0:
	case LAPIC_REG_LVT_PMC:
	case LAPIC_REG_LVT_LINT1:
	case LAPIC_REG_LVT_ERROR:
	{
	    
	    lapic_lvt_t nlint = { raw : value };
	    /* 
	     * jsXXX do this via idx
	     */
	    lapic_lvt_t *lint = 
		(reg == LAPIC_REG_LVT_PMC)   ? &fields.pmc : 
		(reg == LAPIC_REG_LVT_LINT0) ? &fields.lint0 :
		(reg == LAPIC_REG_LVT_LINT1) ? &fields.lint1 : &fields.error;
		
	    lint->x.msk = nlint.x.msk;
	    lint->x.trg = nlint.x.trg;
	    lint->x.pol = nlint.x.pol;
	    lint->x.dlm = nlint.x.dlm;
	    lint->x.vec = nlint.x.vec;
		
	    if(debug_lapic) 
	    {
		const char *lintname = 
		    (reg == LAPIC_REG_LVT_PMC) ? "PMC" : 
		    (reg == LAPIC_REG_LVT_LINT0) ? "LINT0" : 
		    (reg == LAPIC_REG_LVT_LINT1) ? "LINT1" : 
		    "ERROR";
			    
		con << "LAPIC " << get_id() << " set " << lintname << "\n\t\t" 
		    << ((lint->x.msk == 1) ? "masked" : "unmasked") << "\n\t\t"
		    << ((lint->x.pol == 1) ? "low" : " high") << " active\n\t\t" 
		    << " delivery mode to " << lapic_delmode[lint->x.dlm] << "\n\t\t"
		    << " vector to " << lint->x.vec << "\n";
		
	    }
	}
	break;
	case LAPIC_REG_ERR_STATUS:
	{
	    if(debug_lapic) con << "LAPIC " << get_id() << " set ESR to " << (void *) value << "\n";
	}
	break;
	case LAPIC_REG_INTR_CMDLO:
	{
	    lapic_icrlo_reg_t nicrlo = { raw : value };
	    
	    fields.icrlo.x.vector = nicrlo.x.vector;
	    fields.icrlo.x.delivery_mode = nicrlo.x.delivery_mode;
	    fields.icrlo.x.destination_mode = nicrlo.x.destination_mode;
	    fields.icrlo.x.level = nicrlo.x.level;
	    fields.icrlo.x.trigger_mode = nicrlo.x.trigger_mode;
	    fields.icrlo.x.dest_shorthand = nicrlo.x.dest_shorthand;
		    
	    /*
	     * Get IPI destination
	     */
	    ASSERT(vcpu_t::nr_vcpus < 32);
	    word_t dest_id_mask = (1UL << vcpu_t::nr_vcpus);
	    
	    switch (fields.icrlo.x.dest_shorthand)
	    {
		case LAPIC_SH_NO:
		{
		    switch (fields.icrlo.x.destination_mode)
		    {
			case LAPIC_DS_PHYSICAL:
			{
			    dest_id_mask = (1 << fields.icrhi.x.dest);
			}
			break;
			case LAPIC_DS_LOGICAL:
			{
			    /* 
			     * jsXXX: speed up logical destination mode handling
			     */
			    dest_id_mask = 0;
			    for (word_t dest_id = 0; dest_id < vcpu_t::nr_vcpus; dest_id++)
			    {
				local_apic_t &remote_lapic = get_lapic(dest_id);
				if (dest_id != get_id())
				    remote_lapic.lock();
				if (remote_lapic.lid_matches(fields.icrhi.x.dest))
				    dest_id_mask |= (1 << dest_id);
				if (dest_id != get_id())
				    remote_lapic.unlock();

			    }
			}
			break;
			default: 
			{
			    con << "LAPIC " << get_id() << "IPI"				
				<< " INVALID destination mode"
				<< (void *) fields.icrlo.x.destination_mode
				<< " icrlo " << (void *) fields.icrlo.raw 
				<< "\n";
			    DEBUGGER_ENTER(0);
			}
			break;
			

		    }
		}
		break;
		case LAPIC_SH_SELF:
		{
		    dest_id_mask = (1 << get_id());
		}
		break;
		case LAPIC_SH_ALL:
		{
		    dest_id_mask = ((1 << vcpu_t::nr_vcpus) - 1);
		}
		break;
		case LAPIC_SH_ALL_BUT_SELF:
		{
		    dest_id_mask = ((1 << vcpu_t::nr_vcpus) - 1) & ~(1 << get_id());
		}
		break;
		default:
		{
		    con << "LAPIC " << get_id() << " IPI"
			<< " INVALID destination shorthand for IPI\n"
			<< fields.icrlo.x.dest_shorthand
			<< " icrlo " << (void *) fields.icrlo.raw   
 			<< "\n"; 
		    DEBUGGER_ENTER(0);
		}
		break;
	    }
	    
	    if (dest_id_mask >= (1UL << vcpu_t::nr_vcpus))
	    {
		con << "LAPIC " << get_id() << " IPI"
		    << " INVALID physical destination " << (void *) dest_id_mask << " for IPI" 
		    << " icrlo " << (void *) fields.icrlo.raw   
		    << "\n"; 
		DEBUGGER_ENTER(0);
		break;
	    }
		
	    /*
	     * Get IPI delivery mode
	     */
	    switch(fields.icrlo.x.delivery_mode)
	    {
		
		case LAPIC_DM_FIXED:
		{	
		    /*
		     * Fixed IPI
		     */
		    
		    while (dest_id_mask)
		    {
#if defined(CONFIG_VSMP)
			word_t dest_id = lsb(dest_id_mask);
			
			if (debug_lapic)
			    con << "LAPIC " << get_id() << " fixed IPI"
				<< " to " << dest_id
				<< " to mask " << (void *) dest_id_mask
				<< " vector " << fields.icrlo.x.vector 
				<< "\n";
			

			dest_id_mask &= ~(1 << dest_id);
			local_apic_t &remote_lapic = get_lapic(dest_id);
			if (dest_id != get_id())
			    remote_lapic.lock();
			remote_lapic.raise_vector(fields.icrlo.x.vector, INTLOGIC_INVALID_IRQ);
			if (dest_id != get_id())
			    remote_lapic.unlock();
#else
			UNIMPLEMENTED();			
#endif
		    }
		}
		break;
		case LAPIC_DM_INIT:
		{
		    /*
		     * Init IPI (resets ArbID) -- ignore for now
		     */
		    if (debug_lapic)	
			con << "LAPIC " << get_id() << " init IPI"
			    << " to " << (void *) dest_id_mask
			    << " ignore\n";
		}
		break;
		case LAPIC_DM_STARTUP:
		{
		 
		    /*
		     * Startup IPI
		     */
		    while (dest_id_mask)
		    {
#if defined(CONFIG_VSMP)
			word_t dest_id = lsb(dest_id_mask);
			
			if (debug_lapic)
			    con << "LAPIC " << get_id() << " startup IPI"
				<< " to " << dest_id
				<< " to mask " << (void *) dest_id_mask
				<< " vector " << fields.icrlo.x.vector 
				<< " (ip " << (void *) (fields.icrlo.x.vector << 12) << ")"
				<< "\n";
		    
			dest_id_mask &= ~(1 << dest_id);
			
			/*
			 * Startup already in progress?
			 */
			if (get_vcpu().is_booting_other_vcpu())
			{
			    if (debug_lapic)
				con << "LAPIC " << get_id()  << " startup IPI"
				    << " already in progress " << dest_id 
				    << "\n";
			    continue;

			}
			/*
			 * VCPU already started up?
			 */
			if (!get_vcpu(dest_id).is_off())
			{
			    if (debug_lapic)
				con << "LAPIC " << get_id() << " startup IPI"
				    << "  destination VCPU already turned on " << dest_id 
				    << "\n";
			    continue;
			}
			
			/* 
			 * Startup AP 
			 * jsXXX: todo implement ESR
			 */
			word_t startup_ip = afterburn_cpu_get_startup_ip( dest_id );
			if (debug_lapic)
			    con << "LAPIC " << get_id() << " startup IPI"
				<< " real mode ip " << (void *) (fields.icrlo.x.vector << 12)
				<< " using guest provided ip " << (void *) startup_ip
				<< "\n";
			
			get_vcpu(dest_id).startup(startup_ip);	
#else
			UNIMPLEMENTED();
#endif	
			if (debug_lapic)
			    con << "LAPIC " << get_id() << " startup IPI done\n";
		    
		    }
		}
		break;
		default:
		{
		    con << "LAPIC " << get_id() << " IPI"
			<< " UNIMPLEMENTED or INVALID IPI delivery mode " << fields.icrlo.x.delivery_mode 
			<< " to " << (void *) dest_id_mask << " vector " << fields.icrlo.x.vector 
			<< " icrlo " << (void *) fields.icrlo.raw
			<< "\n"; 
		    DEBUGGER_ENTER(0);
		}
		break;
	    }
	}
	break;
	case LAPIC_REG_INTR_CMDHI:
	{
	    
	    lapic_icrhi_reg_t nicrhi = { raw : value };

	    fields.icrhi.x.dest = nicrhi.x.dest;
	    
	    if (debug_lapic) 
		con << "LAPIC " << get_id() 
		    << " set IPI destination " << nicrhi.x.dest << "\n";
	    
	}	
	break;
	case LAPIC_REG_TIMER_DIVIDE:
	{
#warning jsXXX: implement LAPIC timer counter register
	    lapic_divide_reg_t ndiv_conf = { raw : value };
	    
	    fields.div_conf.x.div0 = ndiv_conf.x.div0;
	    fields.div_conf.x.div1 = ndiv_conf.x.div1;
	    fields.div_conf.x.div2 = ndiv_conf.x.div2;
	    
	    if (debug_lapic)
		con << "LAPIC " << get_id() << " setting timer divide to " 
		    << (void *) fields.div_conf.raw << "\n";
	}
	break;
	case LAPIC_REG_TIMER_COUNT:
	{
	    fields.init_count = value;
	    
	    if (debug_lapic)
		con << "LAPIC " << get_id() << " setting current timer value to " << value << "\n";
	}
	break;
    
	default:
	    con << "LAPIC " << get_id() << " write to non-emulated register " 
		<< (void *) reg << ", value " << (void*) value << "\n" ;
	    DEBUGGER_ENTER(0);
	    break;
    }
}
