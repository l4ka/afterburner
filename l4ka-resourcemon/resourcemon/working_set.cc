/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/working_set.cc
 * Description:   Calculate the memory working set of a work load.
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

#include <common/hthread.h>
#include <common/console.h>
#include <resourcemon/resourcemon.h>
#include <resourcemon/vm.h>

#if defined(cfg_perfmon)
#include <common/ia32/perfmon.h>
static benchmarks_t *benchmarks = NULL; // Is assigned k8 or p4 benchmarks depending on CPU ID.
#endif

#if defined(cfg_perfmon_scan)
struct benchmark_set_t {
    static const word_t count = 4;
    unsigned long long tsc;
    unsigned long long counters[count];
};

static benchmark_set_t perfmon_sets[cfg_perfmon_scan_samples];
#endif

#define PRIO_WS_SCAN		(254)

#if defined(cfg_working_set_scan)
typedef struct {
    unsigned short pages;
    unsigned short write;
} ws_depot_t;

static ws_depot_t ws_depot[ cfg_working_set_samples ];
#endif

#if defined(cfg_working_set_trace)
static L4_Word_t ws_page_list[ cfg_working_set_samples ];
static L4_Word_t ws_page_count;
#endif

static hconsole_t ws_cout;

static L4_Word_t working_set_scan( vm_t *vm, 
	L4_Word_t *read, L4_Word_t *write, L4_Word_t *exe )
{
    L4_Word_t ws_page_cnt = 0;
    L4_Word_t max_haddr = vm->get_haddr_base() + vm->get_space_size();

    *read = *write = *exe = 0;

    for( L4_Word_t haddr = vm->get_haddr_base(); haddr < max_haddr; )
    {
	L4_Fpage_t fpages[64];
	L4_Word_t n;

	for( n = 0; (n < 64) && (haddr < max_haddr); n++, haddr += PAGE_SIZE )
	    fpages[n] = L4_Fpage( haddr, PAGE_SIZE );
	
	L4_UnmapFpages( n, fpages );

	for( L4_Word_t i = 0; i < n; i++)
	{
	    if( L4_Rights(fpages[i]) )
	    {
		ws_page_cnt++;
#if defined(cfg_working_set_trace)
		if( ws_page_count < cfg_working_set_samples )
		    ws_page_list[ ws_page_count++ ] = L4_Address(fpages[i]) - vm->get_haddr_base();
#endif
	    }
	    if( L4_Rights(fpages[i]) & 1 )
		*exe += 1;
	    if( L4_Rights(fpages[i]) & 2 )
		*write += 1;
	    if( L4_Rights(fpages[i]) & 4 )
		*read += 1;
	}
    }

    return ws_page_cnt;
}


#if defined(cfg_working_set_trace)
static void
scan_active_pages( L4_Word_t milliseconds, vm_t *vm )
{
    L4_Time_t sleep = L4_TimePeriod( milliseconds * 1000 );
    L4_Word_t read, write, exe;

    // Reset the page references.
    ws_page_count = cfg_working_set_samples;
    working_set_scan( vm, &read, &write, &exe );

    // Wait.
    L4_Sleep( sleep );

    // Collect the page references.
    ws_page_count = 0;
    working_set_scan( vm, &read, &write, &exe );

    if( ws_page_count == cfg_working_set_samples )
	hout << "Too many active pages, more than " << cfg_working_set_samples << '\n';
    else
    {
	hout << "Active pages.\n";
	for( L4_Word_t idx = 0; idx < ws_page_count; idx++ )
	    ws_cout << ws_page_list[idx] << '\n';
    }
}
#endif

IDL4_INLINE void IVMControl_start_active_page_scan_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM,
	const L4_Word_t millisecond_sleep,
	const L4_Word_t target_space_id,
	idl4_server_environment *_env ATTR_UNUSED_PARAM)
// List the addresses of the pages used over a time interval.
{
    vm_t *vm = get_vm_allocator()->space_id_to_vm(target_space_id);
    if( !vm )
    {
	hout << "attempt to scan working set of invalid space.\n";
	return;
    }
#if defined(cfg_working_set_trace)
    scan_active_pages( millisecond_sleep, vm );
#else
    hout << "The working set tracer was not built.\n";
#endif
}
IDL4_PUBLISH_IVMCONTROL_START_ACTIVE_PAGE_SCAN(IVMControl_start_active_page_scan_implementation);


#if defined(cfg_working_set_scan)
static void
working_set_start_scan( L4_Word_t milliseconds, L4_Word_t num_samples, 
	                vm_t *vm )
{
    L4_Time_t sleep = L4_TimePeriod( milliseconds * 1000 );

    for( L4_Word_t i = 0; i < num_samples; i++ )
    {
	L4_Sleep( sleep );

	L4_Word_t read, write, exe;
	ws_depot[i].pages = working_set_scan( vm, &read, &write, &exe );
	ws_depot[i].write = write;
    }
}

static void
working_set_dump_scan( L4_Word_t milliseconds, L4_Word_t num_samples )
{
    ws_cout << "milliseconds,pages,write\n";
    for( L4_Word_t i = 0; i < num_samples; i++ )
    {
	ws_cout << (i+1)*milliseconds;
	ws_cout << ',' << ws_depot[i].pages;
	ws_cout << ',' << ws_depot[i].write;
	ws_cout << '\n';
    }
}
#endif

IDL4_INLINE void IVMControl_start_working_set_scan_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM,
	const L4_Word_t millisecond_sleep,
	const L4_Word_t num_samples,
	const L4_Word_t target_space_id,
	idl4_server_environment *_env ATTR_UNUSED_PARAM)
// Count the total number of pages per time interval.
{
    vm_t *vm = get_vm_allocator()->space_id_to_vm(target_space_id);
    if( !vm )
    {
	hout << "attempt to scan working set of invalid space.\n";
	return;
    }

#if defined(cfg_working_set_scan)
    hout << "Running benchmark with " << num_samples << " samples and " << millisecond_sleep << " ms rate.\n";

    L4_Word_t samples = num_samples > cfg_working_set_samples ? cfg_working_set_samples : num_samples;
    working_set_start_scan( millisecond_sleep, samples, vm );
    working_set_dump_scan( millisecond_sleep, samples );
#else
    hout << "The working set sampler was not built.\n";
#endif
}
IDL4_PUBLISH_IVMCONTROL_START_WORKING_SET_SCAN(IVMControl_start_working_set_scan_implementation);


IDL4_INLINE void IVMControl_set_memballoon_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM, 
	const L4_Word_t size, 
	const L4_Word_t target_space_id, 
	idl4_server_environment *_env ATTR_UNUSED_PARAM)
{
    vm_t *vm = get_vm_allocator()->space_id_to_vm(target_space_id);
    if( vm ) 
    {
	    hout << "Setting memory balloon size to " << size / 1024 << "KB\n";
	    vm->set_memballoon(size);
    }
    else
	hout << "attempt to configure memory balloon of invalid space.\n";
}
IDL4_PUBLISH_IVMCONTROL_SET_MEMBALLOON(IVMControl_set_memballoon_implementation);


IDL4_INLINE void IVMControl_get_space_phys_range_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM,
	const L4_Word_t space_id, 
	L4_Word_t *phys_start,
	L4_Word_t *phys_size,
	idl4_server_environment *_env ATTR_UNUSED_PARAM)
{
    vm_t *vm = get_vm_allocator()->space_id_to_vm( space_id );
    if( vm )
    {
	*phys_start = vm->get_haddr_base();
	*phys_size = vm->get_space_size();
    }
    else
    {
	*phys_start = *phys_size = 0;
    }
}
IDL4_PUBLISH_IVMCONTROL_GET_SPACE_PHYS_RANGE(IVMControl_get_space_phys_range_implementation);

IDL4_INLINE void IVMControl_get_space_block_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM,
	const L4_Word_t space_id,
	const L4_Word_t offset,
	const unsigned int request_size,
	char **data,
	unsigned int *size,
	idl4_server_environment *_env ATTR_UNUSED_PARAM)
{
    *size = 0;
    *data = 0;

    vm_t *vm = get_vm_allocator()->space_id_to_vm( space_id );
    if( !vm )
	return;
    if( offset > vm->get_space_size() )
	return;

    *data = (char *)(vm->get_haddr_base() + offset);

    if( request_size > (vm->get_space_size() - offset) )
	*size = vm->get_space_size() - offset;
    else
	*size = request_size;
    if( *size > MB(4) )
	*size = MB(4);
}
IDL4_PUBLISH_IVMCONTROL_GET_SPACE_BLOCK(IVMControl_get_space_block_implementation);


IDL4_INLINE void IVMControl_start_perfmon_scan_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM, 
	const L4_Word_t millisecond_sleep, 
	const L4_Word_t num_samples,
	idl4_server_environment *_env ATTR_UNUSED_PARAM)
{
#if defined(cfg_perfmon_scan)
    L4_Time_t sleep = L4_TimePeriod( millisecond_sleep * 1000 );

    L4_Word_t tot_samples = num_samples;
    if( tot_samples > cfg_perfmon_scan_samples )
	tot_samples = cfg_perfmon_scan_samples;

    // Clear the counters.
    perfmon_start( benchmarks );
    perfmon_stop( benchmarks );

    hout << "Perfmon scanner: " << millisecond_sleep << " (ms) sample period, "
	 << tot_samples << " samples\n";

    // Collect samples.
    for( word_t i = 0; i < tot_samples; i++ ) {
	perfmon_start( benchmarks );
	L4_Sleep( sleep );
	perfmon_stop( benchmarks );

	perfmon_sets[i].tsc = benchmarks->tsc;
	for( word_t p = 0; p < benchmark_set_t::count; p++ )
	    perfmon_sets[i].counters[p] = benchmarks->benchmarks[p].counter;
    }

    // Print the headers.
    ws_cout << "milliseconds,cycles";
    for( word_t p = 0; p < benchmark_set_t::count; p++ )
	ws_cout << ',' << benchmarks->benchmarks[p].name;
    ws_cout << '\n';

    // Print the samples.
    for( word_t i = 0; i < tot_samples; i++ )
    {
	ws_cout << (i+1) * millisecond_sleep;
	ws_cout << ',' << perfmon_sets[i].tsc;
	for( word_t p = 0; p < benchmark_set_t::count; p++ )
	    ws_cout << ',' << perfmon_sets[i].counters[p];
	ws_cout << '\n';
    }
#else
    hout << "Perfmon scanner is disabled.\n";
#endif
}
IDL4_PUBLISH_IVMCONTROL_START_PERFMON_SCAN(IVMControl_start_perfmon_scan_implementation);


static void working_set_thread( 
	void *param ATTR_UNUSED_PARAM,
	hthread_t *htread ATTR_UNUSED_PARAM)
{
    static void *IVMControl_vtable[IVMCONTROL_DEFAULT_VTABLE_SIZE] = IVMCONTROL_DEFAULT_VTABLE;
    L4_ThreadId_t partner;
    L4_MsgTag_t msgtag;
    idl4_msgbuf_t msgbuf;
    long cnt;

    register_interface( UUID_IVMControl, L4_Myself() );

    idl4_msgbuf_init(&msgbuf);

    while (1)
    {
    	partner = L4_nilthread;
      	msgtag.raw = 0;
	cnt = 0;

	while (1)
	{
	    idl4_msgbuf_sync(&msgbuf);

	    idl4_reply_and_wait(&partner, &msgtag, &msgbuf, &cnt);

	    if (idl4_is_error(&msgtag))
		break;

	    idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, IVMControl_vtable[idl4_get_function_id(&msgtag) & IVMCONTROL_FID_MASK]);
	}
    }
}

void IVMControl_discard()
{
}


bool working_set_init()
{
    hthread_t *ws_thread = get_hthread_manager()->create_thread( 
	    hthread_idx_working_set, PRIO_WS_SCAN,
	    working_set_thread );
    if( !ws_thread )
	return false;

    ws_cout.init( hout.get_driver(), NULL );
    hout << "Working set scanner TID: " << ws_thread->get_global_tid() << '\n';

    ws_thread->start();
    return true;
}

static void do_wrmsr( const unsigned int reg, const unsigned long long val )
{
    __asm__ __volatile__ ( "wrmsr" : : "A"(val), "c"(reg) );
}

void perfmon_init()
{
#if defined(cfg_perfmon)
    benchmarks = perfmon_arch_init();
    if( !benchmarks )
	return;
    perfmon_setup( benchmarks, do_wrmsr );
    hout << "Perfmance counters are configured.\n";
#endif
}

