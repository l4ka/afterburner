/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/resourcemon.cc
 * Description:   The main initialization and sequencing code for the
 * 		  resourcemon.
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

#include <l4/kip.h>
#include <l4/thread.h>
#include <l4/message.h>
#include <l4/ipc.h>
#include <l4/sigma0.h>

#include <resourcemon/resourcemon.h>
#include <resourcemon/vm.h>
#include <resourcemon/module_manager.h>
#include <common/console.h>
#include <common/hthread.h>
#include <common/string.h>
#if defined(cfg_logging)
#include <resourcemon/logging.h>
#endif
#if defined(cfg_earm)
#include <resourcemon/earm.h>
#endif

#include <resourcemon/working_set.h>
#include <resourcemon/page_tank.h>
#if defined(cfg_eacc)
#include <resourcemon/eacc.h>
#endif


vm_allocator_t vm_allocator;
page_tank_t page_tank;
L4_Word_t resourcemon_max_phys_addr = 0, resourcemon_tot_mem = 0;

#if defined(cfg_eacc)
eacc_t eacc;
#endif

IDL4_INLINE void IResourcemon_client_init_complete_implementation(
    CORBA_Object _caller, idl4_server_environment *_env)
{
    hout << "Virtual machine init complete.\n";
    if( get_module_manager()->next_module() )
	get_module_manager()->load_current_module();
}
IDL4_PUBLISH_IRESOURCEMON_CLIENT_INIT_COMPLETE(IResourcemon_client_init_complete_implementation);


bool sigma0_request_region( L4_Word_t start, L4_Word_t end )
{
    L4_Fpage_t request_fp, result_fp;

    for( L4_Word_t page = start & PAGE_MASK; page < end; page += PAGE_SIZE )
    {
	request_fp = L4_FpageLog2(page, PAGE_BITS);
	result_fp = L4_Sigma0_GetPage( L4_Pager(), request_fp, request_fp );
	if( L4_IsNilFpage(result_fp) )
	    return false;
    }

    return true;
}

static void find_max_phys_mem()
{
    void *kip = L4_GetKernelInterface();
    L4_Word_t i, max = 0;

    for( i = 0; i < L4_NumMemoryDescriptors(kip); i++ )
    {
	L4_MemoryDesc_t *mdesc = L4_MemoryDesc(kip, i);
	if( L4_IsVirtual(mdesc) )
	    continue;
	if( (L4_Type(mdesc) == L4_ConventionalMemoryType) &&
		(L4_High(mdesc) > max) )
	    max = L4_High(mdesc);
    }

    set_max_phys_addr(max);

    hout << "Maximum useable byte address: " << (void *)max << '\n';
}

bool kip_conflict( L4_Word_t start, L4_Word_t size, L4_Word_t *next )
{
    void *kip = L4_GetKernelInterface();
    L4_Word_t i;
    L4_Word_t end = size - 1 + start;

    for( i = 0; i < L4_NumMemoryDescriptors(kip); i++ )
    {
	L4_MemoryDesc_t *mdesc = L4_MemoryDesc(kip, i);
	if( L4_IsVirtual(mdesc) ||
		(L4_Type(mdesc) == L4_ConventionalMemoryType) )
	    continue;
	if( (L4_Type(mdesc) == L4_SharedMemoryType) && !L4_Low(mdesc) &&
		!(L4_High(mdesc)+1) )
	    continue;	// bogus memory descriptor

	*next = L4_High(mdesc) + 1;

	if( (start < L4_Low(mdesc)) && (end >= L4_Low(mdesc)) )
	    return true;
	if( (start < L4_High(mdesc)) && (end >= L4_High(mdesc)) )
	    return true;
	if( (start >= L4_Low(mdesc)) && (end <= L4_High(mdesc)) )
	    return true;
    }

    return false;
}

static void grab_all_memory( )
{
    L4_Word_t tsize = 0;
    hout << "Finding memory ";
#if defined(CONFIG_ARCH_IA64)
    for (L4_Word_t s = 12; s >= 10; s--)
#else
    for (L4_Word_t s = sizeof (L4_Word_t) * 8 - 1; s >= 10; s--)
#endif
    {
	L4_Fpage_t f;
	int n = -1;

	hout << ".";
	do {
	    f = L4_Sigma0_GetAny (L4_nilthread, s, L4_CompleteAddressSpace);
	    n++;
	} while (! L4_IsNilFpage (f));

	L4_Word_t size = n * (1UL << s);
	tsize += size;
    }
    hout << "\n";

    hout << "Total memory: ";
    hout <<          float(tsize) / float(GB(1)) << " GB";
    hout << " | " << float(tsize) / float(MB(1)) << " MB";
    hout << " | " << float(tsize) / float(KB(1)) << " KB";
    hout << '\n';

    set_tot_mem(tsize);
}

static void request_special_memory( void )
{
#if defined(__i386__)
     for( L4_Word_t page = 0; page < SPECIAL_MEM; page += PAGE_SIZE )
     {
	L4_Fpage_t req_fp = L4_FpageLog2( page, PAGE_BITS );
	L4_Sigma0_GetPage( L4_Pager(), req_fp, req_fp );
     }
#endif
}


void l4_dump_features( )
{
    void *kip = L4_GetKernelInterface();
    char *name;
    hout << "L4 features:\n";
    for( L4_Word_t i = 0; (name = L4_Feature(kip,i)) != '\0'; i++ )
	hout << "\t\t\t" << name << "\n";
    
}


bool l4_has_feature( char *feature_name )
{
    void *kip = L4_GetKernelInterface();
    char *name;

    for( L4_Word_t i = 0; (name = L4_Feature(kip,i)) != '\0'; i++ )
	if( !strcmp(feature_name, name) )
	    return true;
    return false;
}

static void kdebug_putc( const char c )
{
    L4_KDB_PrintChar( c );
}

static void resourcemon_console_init()
{
#if defined(cfg_console_serial)
    static hiostream_arch_serial_t con_driver;
    con_driver.init (hiostream_arch_serial_t::com0);
#elif defined(cfg_console_kdebug)
    static hiostream_kdebug_t con_driver;
    con_driver.init();
    console_init( kdebug_putc, "\e[1m\e[33mresourcemon:\e[0m " );
#else
#warning No console configured.  Console output disabled.
    static hiostream_null_t con_driver;
    con_driver.init();
#endif

    hout.init (&con_driver, "resourcemon: ");
    hout << "Console initialized.\n";
}


int main( void )
{
    // Initialize the resourcemon console.
    resourcemon_console_init();

    // Initialize memory.
    request_special_memory();
    grab_all_memory();
    find_max_phys_mem();

    // Initialize VM state managers.
    tid_space_t::init();
    get_vm_allocator()->init();
    get_hthread_manager()->init();

#if defined(cfg_eacc)
    // Initialize performance counters
    eacc.init();
#else
    // Initialize secondary services.
    perfmon_init();
    working_set_init();
#endif

    extern void client_console_init();
    client_console_init();

    extern void pager_init();
    pager_init();

#if defined(cfg_l4ka_vmextensions)
    extern void virq_init();
    virq_init();
#endif
#if defined(cfg_logging)
    logging_init();
#endif
#if defined(cfg_earm)
    earm_init();
#endif

    l4_dump_features();

   
    // Start loading initial modules.
    if( !get_module_manager()->init() )
	hout << "No virtual machine boot modules found.\n";
    else
	get_module_manager()->load_current_module();

    // Enter the server loop.
    hout << "Entering the server loop ...\n";
    extern void IResourcemon_server(void);
    IResourcemon_server();
}
