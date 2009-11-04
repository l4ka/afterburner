/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon.cc
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

#include <logging.h>
#include <resourcemon.h>
#include <vm.h>
#include <module_manager.h>
#include <hthread.h>
#include <string.h>
#include <virq.h>
#include <earm.h>

#include <working_set.h>
#include <page_tank.h>


vm_allocator_t vm_allocator;
page_tank_t page_tank;
L4_Word_t resourcemon_max_phys_addr = 0, resourcemon_tot_mem = 0;

IDL4_INLINE void IResourcemon_client_init_complete_implementation(
    CORBA_Object _caller, idl4_server_environment *_env)
{
    printf( "Virtual machine init complete.\n");
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
    L4_Word_t i, max = 0;

    for( i = 0; i < L4_NumMemoryDescriptors(l4_kip); i++ )
    {
	L4_MemoryDesc_t *mdesc = L4_MemoryDesc(l4_kip, i);
	if( L4_IsVirtual(mdesc) )
	    continue;
	if( (L4_Type(mdesc) == L4_ConventionalMemoryType) &&
		(L4_High(mdesc) > max) )
	    max = L4_High(mdesc);
    }

    set_max_phys_addr(max);

    printf( "Maximum useable physical memory address: %x\n", max);
}

bool kip_conflict( L4_Word_t start, L4_Word_t size, L4_Word_t *next )
{
    L4_Word_t i;
    L4_Word_t end = size - 1 + start;

    for( i = 0; i < L4_NumMemoryDescriptors(l4_kip); i++ )
    {
	L4_MemoryDesc_t *mdesc = L4_MemoryDesc(l4_kip, i);
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
    printf( "Finding memory ");
    
    for (L4_Word_t s = sizeof (L4_Word_t) * 8 - 1; s >= 10; s--)
    {
	L4_Fpage_t f;
	int n = -1;

	printf( ".");
	do {
	    f = L4_Sigma0_GetAny (L4_nilthread, s, L4_CompleteAddressSpace);
	    n++;
	} while (! L4_IsNilFpage (f));

	L4_Word_t size = n * (1UL << s);
	tsize += size;
    }
    printf( "\n");

    printf( "Total memory: %f GB | %f MB | %f KB\n",
	    float(tsize) / float(GB(1)),
	    float(tsize) / float(MB(1)),
	    float(tsize) / float(KB(1)));

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



bool l4_has_feature( char *feature_name )
{
    char *name;

    for( L4_Word_t i = 0; (name = L4_Feature(l4_kip,i)) != '\0'; i++ )
	if( !strcmp(feature_name, name) )
	    return true;
    return false;
}


int main( void )
{
    l4_kip = L4_GetKernelInterface();
    l4_cpu_cnt = L4_NumProcessors(l4_kip);
    l4_user_base = L4_ThreadIdUserBase(l4_kip);

    char *l4_feature;
    printf( "L4 features:\n");
    
    for( L4_Word_t i = 0; (l4_feature = L4_Feature(l4_kip,i)) != '\0'; i++ )
	printf( "\t\t\t%s\n", l4_feature);

    
    // Initialize memory.
    request_special_memory();
    grab_all_memory();
    find_max_phys_mem();

	
    // Initialize VM state managers.
    tid_space_t::init();
    get_vm_allocator()->init();
    get_hthread_manager()->init();
    
    if (l4_pmsched_enabled)
        virq_init();

    console_init();
    
    if (!l4_pmsched_enabled)
	working_set_init();

    pager_init();
    
    logging_init();

    extern void pmc_setup();
    pmc_setup();

#if defined(cfg_earm) 
    earm_init();
#endif
   
    
    
    // Start loading initial modules.
    if ( !get_module_manager()->init() )
	printf( "No virtual machine boot modules found.\n");
    else
	get_module_manager()->load_current_module();

    // Enter the server loop.
    extern void IResourcemon_server(void);
    IResourcemon_server();
}
