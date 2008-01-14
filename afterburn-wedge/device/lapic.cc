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
#include <debug.h>
#include <device/lapic.h>
#include <bind.h>

const char *lapic_delmode[8] =
{ "fixed", "reserved", "SMI", "reserved", 
  "NMI", "INIT", "reserved", "ExtINT" };

extern "C" void __attribute__((regparm(2))) lapic_write_patch( word_t value, word_t addr )
{
    local_apic_t &lapic = get_lapic();
    UNUSED word_t dummy;
    
    ASSERT(lapic.get_id() == get_vcpu().cpu_id);

    if( debug_apic_sanity )
    {
	pgent_t *pgent = backend_resolve_addr(addr, dummy);
	
	if( !pgent || !pgent->is_kernel() )
	    PANIC( "Inconsistent guest pagetable entries for IO-APIC mapping @ %x pgent %x\n",
		    addr, pgent);
	
	if (!lapic.is_valid_lapic())
	    PANIC( "BUG no sane softLAPIC @ %x phys %x pgent %x\n",
		     &lapic, pgent->get_address(), pgent);
    }

    dprintf(debug_apic, "APIC %d write reg %x addr %x (%x) value %x IP %x\n",
	    lapic.get_id(), lapic.addr_to_reg(addr), addr, 
	    backend_resolve_addr(addr, dummy)->get_address(),
	    value, __builtin_return_address(0));

    lapic.write(value, lapic.addr_to_reg(addr));
}

extern "C" word_t __attribute__((regparm(1))) lapic_read_patch( word_t addr )
{
    local_apic_t &lapic = get_lapic();
    UNUSED word_t dummy;
    
    ASSERT(lapic.get_id() == get_vcpu().cpu_id);

    if (lapic.addr_to_reg(addr) == local_apic_t::LAPIC_REG_TIMER_COUNT)
    {
	printf( "LAPIC reading TIMER COUNT UNIMPLEMENTED -- must be computed!\n");
	//DEBUGGER_ENTER(0);
    }
    
    lapic.lock();
    word_t ret = lapic.get_reg(lapic.addr_to_reg(addr));
    lapic.unlock();
    
    if( debug_apic_sanity )
    {
	pgent_t *pgent = backend_resolve_addr(addr, dummy);
	
	if( !pgent || !pgent->is_kernel() )
	    PANIC( "Inconsistent guest pagetable entries for IO-APIC mapping @ %x pgent %x\n",
		    addr, pgent);
	
	if (!lapic.is_valid_lapic())
	    PANIC( "BUG no sane softLAPIC @ %x phys %x pgent %x\n",
		     &lapic, pgent->get_address(), pgent);
    }
    
    dprintf(debug_apic, "APIC %d read reg %x addr %x (%x) value %x IP %x\n",
	    lapic.get_id(), lapic.addr_to_reg(addr), addr, 
	    backend_resolve_addr(addr, dummy)->get_address(), 
	    ret,  __builtin_return_address(0));

    
    return ret;

}

extern "C" word_t __attribute__((regparm(2))) lapic_xchg_patch( word_t value, word_t addr )
{
    // TODO: confirm that the address maps to our bus address.  And determine
    // the  device from the address.
    local_apic_t &lapic = get_lapic();
    UNUSED word_t dummy;
    
    ASSERT(lapic.get_id() == get_vcpu().cpu_id);
    
    lapic.lock();
    word_t ret = lapic.get_reg(lapic.addr_to_reg(addr));
    lapic.write(value, lapic.addr_to_reg(addr));
    lapic.unlock();
    
    printf("APIC %d UNTESTED read reg %x addr %x (%x) value %x IP %x\n",
	   lapic.get_id(), lapic.addr_to_reg(addr), addr, 
	   backend_resolve_addr(addr, dummy)->get_address(), ret,  __builtin_return_address(0));
    DEBUGGER_ENTER("APIC");
    return ret;

}

bool local_apic_t::pending_vector( word_t &vector, word_t &irq)
{
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
	
	UNUSED i82093_t *from = get_ioapic(vector);
	dprintf(irq_dbg_level(irq, vector), "LAPIC %d found pending vector in IRR %d ISR %x\n", get_id(), vector, vector_in_service); 
	dprintf(irq_dbg_level(irq, vector), "IRQ %d IOAPIC %d\n", irq, (from ? from->get_id():  (word_t) -1));
	
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

	dprintf(irq_dbg_level(irq, vector),
		"LAPIC %d raise vector %d irq %d from IOAPIC %d from_eoi %d reraise %d\n",
		get_id(), vector, irq, (from ? from->get_id(): (word_t) -1), from_eoi, reraise);
    }
}


void local_apic_t::write(word_t value, word_t reg)
{
    ASSERT(get_vcpu().cpu_id == get_id());
	    
    switch (reg) {
	
    case LAPIC_REG_VER:
    {
	dprintf(debug_apic, "LAPIC %d write to r/o version register %x\n", get_id(), value);
    }
    break;
    case LAPIC_REG_LDR:
    {
	lapic_id_reg_t nid = { raw: value };
	dprintf(debug_apic, "LAPIC %d set logical dest to %x\n", get_id(), nid.x.id);
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
	    dprintf(debug_apic, "LAPIC %d set dfr to flat model\n", get_id());
	}
	break;
	case LAPIC_DF_CLUSTER:
	{
	    printf("LAPIC %d UNIMPLEMENTED dfr cluster format model\n", get_id());
	    DEBUGGER_ENTER("IOAPIC");
	}
	break;
	default:
	{
	    printf("LAPIC %d INVALID dfr format\n", get_id());
	    DEBUGGER_ENTER("IOAPIC");
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
	    dprintf(debug_apic, "LAPIC %d set task prio to accept all\n", get_id());
	    return;
	}
	else
	{
	    dprintf(debug_apic, "LAPIC %d UNIMPLEMENTED task priorities %x/%x\n", 
		    get_id(), ntpr.x.prio, ntpr.x.subprio);
	    DEBUGGER_ENTER("LAPIC");
	}
    }
    break;
    case LAPIC_REG_EOI:
    {
    
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
		
	    dprintf(irq_dbg_level(irq, vector), "LAPIC %d EOI vector %d IOAPIC %d\n", 
		    get_id(), vector, (get_ioapic(vector) ? get_ioapic(vector)->get_id() :  (word_t)  -1));
		
		
	}
    }		    
    break;
    case LAPIC_REG_SVR:
    {
	lapic_svr_reg_t nsvr = { raw : value };

	fields.svr.x.vector = nsvr.x.vector;
	fields.svr.x.focus_processor = nsvr.x.focus_processor;

	dprintf(debug_apic, "LAPIC %d set spurious vector to %d\n", get_id(), fields.svr.x.vector);
	dprintf(debug_apic, "LAPIC %d set focus processor to %d\n", get_id(), fields.svr.x.focus_processor);
		
	if ((fields.svr.x.enabled = nsvr.x.enabled))
	{
	    dprintf(debug_apic, "LAPIC %d enabled\n", get_id());
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
	    printf("LAPIC %d disable UNIMPLEMENTED\n", get_id());
	    DEBUGGER_ENTER("LAPIC");
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

	dprintf(debug_apic, "LAPIC %d set TIMER %s vector %d\n", get_id(),
		((ntimer.x.msk == 1) ? "masked" : "unmasked"), ntimer.x.vec);
	    
	if (ntimer.x.mod == 0 && ntimer.x.msk == 0)
	{
	    printf( "LAPIC %d timer mode (one-shot) UNIMPLEMENTED\n", get_id());
	    DEBUGGER_ENTER("LAPIC");
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
	    
	UNUSED const char *lintname = 
	    (reg == LAPIC_REG_LVT_PMC) ? "PMC" : 
	    (reg == LAPIC_REG_LVT_LINT0) ? "LINT0" : 
	    (reg == LAPIC_REG_LVT_LINT1) ? "LINT1" : 
	    "ERROR";
	
	dprintf(debug_apic, "LAPIC %d set %s %s %s active delivery mode %s vector %d\n",
		get_id(), lintname, 
		((lint->x.msk == 1) ? "masked" : "unmasked"),
		((lint->x.pol == 1) ? "low" : " high"),
		 lapic_delmode[lint->x.dlm], lint->x.vec);
		
    }
    break;
    case LAPIC_REG_ERR_STATUS:
    {
	dprintf(debug_apic, "LAPIC %d set ESR to %x\n", get_id(), value);
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
		dprintf(debug_apic, "LAPIC %d invalid destination mode %x icrlo %x\n",
			get_id(), fields.icrlo.x.destination_mode, fields.icrlo.raw);
		DEBUGGER_ENTER("LAPIC");
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
	    dprintf(debug_apic, "LAPIC %d IPI invalid destination shorthand %x icrlo %x\n", 
		    get_id(), fields.icrlo.x.dest_shorthand, fields.icrlo.raw);
	    DEBUGGER_ENTER("LAPIC");
	}
	break;
	}
	    
	if (dest_id_mask >= (1UL << vcpu_t::nr_vcpus))
	{
	    dprintf(debug_apic, "LAPIC %d IPI invalid physical destination %x icrlo %x\n", 
		    get_id(), dest_id_mask, fields.icrlo.raw);
	    DEBUGGER_ENTER("LAPIC");
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
		word_t dest_id = lsb(dest_id_mask);
			
		dprintf(debug_apic, "LAPIC %d IPI fixed dest to %x to mask %x vector %d\n",
			get_id(), dest_id, dest_id_mask, fields.icrlo.x.vector);
			

		dest_id_mask &= ~(1 << dest_id);
		
		if (dest_id < vcpu_t::nr_vcpus)
		{
		    local_apic_t &remote_lapic = get_lapic(dest_id);
		    if (dest_id != get_id())
			remote_lapic.lock();
		    remote_lapic.raise_vector(fields.icrlo.x.vector, INTLOGIC_INVALID_IRQ);
		    if (dest_id != get_id())
			remote_lapic.unlock();
		}
	    }
	}
	break;
	case LAPIC_DM_INIT:
	{
	    /*
	     * Init IPI (resets ArbID) -- ignore for now
	     */
	    dprintf(debug_apic, "LAPIC %d IPI init dest to mask %x vector %d, ignore\n",
		    get_id(), dest_id_mask, fields.icrlo.x.vector);
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
			
		dprintf(debug_apic, "LAPIC %d IPI startup to %x to mask %x vector %d\n",
			get_id(), dest_id, dest_id_mask, fields.icrlo.x.vector);
			
		dest_id_mask &= ~(1 << dest_id);
			
		/*
		 * Startup already in progress?
		 */
		if (get_vcpu().is_booting_other_vcpu())
		{
		    dprintf(debug_apic, "LAPIC %d IPI startup IPI dest %d already in progress\n", 
			    get_id(), dest_id);
		    continue;
		}
		/*
		 * VCPU already started up?
		 */
		if (!get_vcpu(dest_id).is_off())
		{
		    dprintf(debug_apic, "LAPIC %d IPI startup IPI dest %d VCPU %d already turned on\n", 
			    get_id(), dest_id, get_vcpu(dest_id).cpu_id);
		    continue;
		}
			
		/* 
		 * Startup AP 
		 * jsXXX: todo implement ESR
		 */
		word_t startup_ip = afterburn_cpu_get_startup_ip( dest_id );
		dprintf(debug_apic, "LAPIC %d IPI startup to %x real mode IP %x guest IP %d\n",
			get_id(), dest_id, (fields.icrlo.x.vector << 12), startup_ip);
			
		get_vcpu(dest_id).startup(startup_ip);	
#else
		UNIMPLEMENTED();
#endif	
		dprintf(debug_apic, "LAPIC %d startup IPI done\n", get_id());
		    
	    }
	}
	break;
	default:
	{
	    printf("LAPIC %d IPI UNIMPLEMENTED or INVALID IPI delivery mode %x to mask %x vector %x icrlo %x\n",
		   get_id(), fields.icrlo.x.delivery_mode, dest_id_mask, fields.icrlo.x.vector, fields.icrlo.raw);
	    DEBUGGER_ENTER("LAPIC");
	}
	break;
	}
    }
    break;
    case LAPIC_REG_INTR_CMDHI:
    {
	    
	lapic_icrhi_reg_t nicrhi = { raw : value };

	fields.icrhi.x.dest = nicrhi.x.dest;
	    
	dprintf(debug_apic, "LAPIC %d set IPI destination %x\n", get_id(), nicrhi.x.dest);
	    
    }	
    break;
    case LAPIC_REG_TIMER_DIVIDE:
    {
#warning jsXXX: implement LAPIC timer counter register
	lapic_divide_reg_t ndiv_conf = { raw : value };
	    
	fields.div_conf.x.div0 = ndiv_conf.x.div0;
	fields.div_conf.x.div1 = ndiv_conf.x.div1;
	fields.div_conf.x.div2 = ndiv_conf.x.div2;
	    
	dprintf(debug_apic, "LAPIC %d set timer divide to %x\n", get_id(), fields.div_conf.raw);
    }
    break;
    case LAPIC_REG_TIMER_COUNT:
    {
	fields.init_count = value;
	dprintf(debug_apic, "LAPIC %d set current timer val to %x\n", get_id(), value);
    }
    break;
    
    default:
	printf( "LAPIC %d write to non-emulated register %x value %x\n", get_id(), reg, value);
	DEBUGGER_ENTER("LAPIC");
	break;
    }
}
