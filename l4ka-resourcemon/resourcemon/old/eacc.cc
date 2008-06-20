/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/eacc.cc
 * Description:   Energy accounting functions
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

#include <l4/types.h>
#include <resourcemon/resourcemon.h>
#include <resourcemon/vm.h>
#include <resourcemon/eacc.h>
#include "resourcemon_idl_server.h"

void eacc_t::print()
{
    volatile L4_Word64_t buf;
    for (int j = 0; j < 18; ++j)
    {
	buf = x86_rdpmc(j);
	printf( "pmc %d buf %x mask %x\n", j, buf, pmc_mask[j]);
    }

    buf = x86_rdtsc();
    printf( "tsc   buf %x mask %x\n", buf);

}

void eacc_t::init()
{
    printf( "Initialize energy accounting\n");
    word_t mask = ~0UL ^ (fpage_size - 1);
    word_t distance = sizeof(L4_Word64_t) * NR_WEIGHTS;

    pmc_diff_energy[0] = reinterpret_cast<L4_Word64_t*>(reinterpret_cast<word_t>(&pmc_mapping[PAGE_SIZE]) & mask);
    pmc_raw_energy[0] = reinterpret_cast<L4_Word64_t*>(reinterpret_cast<word_t>(pmc_diff_energy[0]) + distance * nr_pcpus);

    for (word_t i = 1; i < nr_pcpus; ++i)
    {
	pmc_diff_energy[i] = reinterpret_cast<L4_Word64_t*>(reinterpret_cast<word_t>(pmc_diff_energy[i-1]) + distance);
	pmc_raw_energy[i] = reinterpret_cast<L4_Word64_t*>(reinterpret_cast<word_t>(pmc_raw_energy[i-1]) + distance);
    }

    for (word_t i = 0; i < nr_pcpus; ++i)
    {
	for (word_t j = 0; j < NR_WEIGHTS; ++j)
	{
	    pmc_diff_energy[i][j] = 0;
	    pmc_raw_energy[i][j] = 0;
	    pmc_raw[i][j] = 0;
	}
    }

    for (word_t i = 0; i < nr_pm_counters; ++i)
	pmc_mask[i] = 0;
}

void eacc_t::write_msr(const word_t addr, const L4_Word64_t buf)
{
    x86_wrmsr(addr, buf);
    if (addr >= MSR_IA32_BPU_CCCR0 && addr <= MSR_IA32_IQ_CCCR5)
	pmc_mask[addr - MSR_IA32_BPU_CCCR0] = static_cast<word_t>(buf) & 0x00001000;
}


void eacc_t::pmc_setup()
{
    L4_Word64_t buf = 0x30000;

    // reset performance counters
    for (word_t addr=MSR_IA32_BPU_CCCR0; addr <= MSR_IA32_IQ_CCCR5; ++addr)
    {
	buf = 0x30000;
	x86_wrmsr(addr, buf);
    }

    buf = 0;
    for (word_t addr=MSR_IA32_BPU_COUNTER0; addr <= MSR_IA32_IQ_COUNTER5; ++addr)
	x86_wrmsr(addr, buf);

    // Configure ESCRs

    buf = ((L4_Word64_t)0x0 << 32) | 0xFC00;
    x86_wrmsr(MSR_IA32_TC_PRECISE_EVENT, buf);

    // Enable Precise Event Based Sampling (accurate & low sampling overhead)
    buf = ((L4_Word64_t)0x0 << 32) | 0x1000001;
    x86_wrmsr(MSR_IA32_PEBS_ENABLE, buf);

    // Also enabling PEBS
    buf = ((L4_Word64_t)0x0 << 32) | 0x1;
    x86_wrmsr(MSR_IA32_PEBS_MATRIX_VERT, buf);

    // Count unhalted cycles
    buf = ((L4_Word64_t)0x0 << 32) | 0x2600020C;
    x86_wrmsr(MSR_IA32_FSB_ESCR0, buf);

    // Count load uops that are replayed due to unaligned addresses
    // and/or partial data in the Memory Order Buffer (MOB)
    buf = ((L4_Word64_t)0x0 << 32) | 0x600740C;
    x86_wrmsr(MSR_IA32_MOB_ESCR0, buf);

    // Count op queue writes
    buf = ((L4_Word64_t)0x0 << 32) | 0x12000E0C;
    x86_wrmsr(MSR_IA32_MS_ESCR0, buf);

    // Count retired branches
    buf = ((L4_Word64_t)0x0 << 32) | 0x8003C0C;
    x86_wrmsr(MSR_IA32_TBPU_ESCR0, buf);

    // Count x87_FP_uop 
    buf = ((L4_Word64_t)0x0 << 32) | 0x900000C;
    x86_wrmsr(MSR_IA32_FIRM_ESCR0, buf);

    // Count mispredicted
    buf = ((L4_Word64_t)0x0 << 32) | 0x600020C;
    x86_wrmsr(MSR_IA32_CRU_ESCR0, buf);

    // Count memory retired
    buf = ((L4_Word64_t)0x0 << 32) | 0x1000020C;
    x86_wrmsr(MSR_IA32_CRU_ESCR2, buf);

    // Count load miss level 1 data cache
    buf = ((L4_Word64_t)0x0 << 32) | 0x1200020C;
    x86_wrmsr(MSR_IA32_CRU_ESCR3, buf);

    // uop type
    buf = ((L4_Word64_t)0x0 << 32) | 0x4000C0C;
    x86_wrmsr(MSR_IA32_RAT_ESCR0, buf);
    // Configure CCCRs

    // Store unhalted cycles
    buf = ((L4_Word64_t)0x0 << 32) | 0x3D000;
    write_msr(MSR_IA32_BPU_CCCR0, buf);

    // Store MOB load replay
    buf = ((L4_Word64_t)0x0 << 32) | 0x35000;
    x86_wrmsr(MSR_IA32_BPU_CCCR1, buf);

    // Store op queue writes
    buf = ((L4_Word64_t)0x0 << 32) | 0x31000;
    x86_wrmsr(MSR_IA32_MS_CCCR0, buf);

    // Store retired branches
    buf = ((L4_Word64_t)0x0 << 32) | 0x35000;
    write_msr(MSR_IA32_MS_CCCR1, buf);

    // Store x87_FP_uop
    buf = ((L4_Word64_t)0x0 << 32) | 0x33000;
    x86_wrmsr(MSR_IA32_FLAME_CCCR0, buf);

    // Store mispredicted branches
    buf = ((L4_Word64_t)0x0 << 32) | 0x39000;
    x86_wrmsr(MSR_IA32_IQ_CCCR0, buf);

    // Store memory retired
    buf = ((L4_Word64_t)0x0 << 32) | 0x3B000;
    write_msr(MSR_IA32_IQ_CCCR1, buf);

    // Store load miss level 1 data cache
    buf = ((L4_Word64_t)0x0 << 32) | 0x3B000;
    write_msr(MSR_IA32_IQ_CCCR2, buf);

    // Store uop type
    buf = ((L4_Word64_t)0x0 << 32) | 0x35000;
    x86_wrmsr(MSR_IA32_IQ_CCCR4, buf);

    // Setup complete

}

void eacc_t::write(word_t pcpu)
{
    L4_Word64_t buf, result, max_value;
    max_value = (static_cast<L4_Word64_t>(1) << 40);
    int j = 0;
    for (word_t i = 0; i < nr_pm_counters; ++i)
    {
	if (pmc_mask[i])
	{
	    buf = x86_rdpmc(i);
	    result = buf - pmc_raw[pcpu][j];
	    if (EXPECT_FALSE(result >= max_value))
	    {
		// overflow
		result = max_value - pmc_raw[pcpu][j] + buf;
	    }
	    ASSERT(pmc_weight[j]);
	    pmc_diff_energy[pcpu][j] = result *  pmc_weight[j];
	    pmc_raw_energy[pcpu][j] = buf * pmc_weight[j];
	    pmc_raw[pcpu][j++] = buf;
	}
    }

    buf = x86_rdtsc();
    result = buf - pmc_raw[pcpu][j];
    if (EXPECT_FALSE(result >= max_value))
    {
	// overflow
	result = max_value - pmc_raw[pcpu][j] + buf;
    }
    pmc_diff_energy[pcpu][j] = result * pmc_weight[j];
    pmc_raw_energy[pcpu][j] = buf * pmc_weight[j];
    pmc_raw[pcpu][j] = buf;
    ASSERT(j == NR_WEIGHTS - 1);
}


void eacc_t::map(idl4_fpage_t *fp)
{
    idl4_fpage_set_base(fp, 0);
    idl4_fpage_set_page(fp, L4_Fpage(get_pmc_mapping_start_addr(), get_sizeof_fpage()));
    idl4_fpage_set_mode(fp, IDL4_MODE_MAP);
    idl4_fpage_set_permissions(fp, IDL4_PERM_READ);
}

#if defined(cfg_eacc)
IDL4_INLINE void  IResourcemon_request_performance_counter_pages_implementation(
    CORBA_Object  _caller,
    idl4_fpage_t * fp,
    idl4_server_environment * _env)

{
  /* implementation of IResourcemon::request_performance_counter_pages */
    if(!get_vm_allocator()->tid_to_vm(_caller))
    {
	printf( "unknown client %t\n", _caller);
	CORBA_exception_set( _env, ex_IResourcemon_unknown_client, NULL );
	return;
    }

    eacc.map(fp);
}

IDL4_PUBLISH_IRESOURCEMON_REQUEST_PERFORMANCE_COUNTER_PAGES(IResourcemon_request_performance_counter_pages_implementation);

#endif
