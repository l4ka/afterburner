/*********************************************************************
 *                
 * Copyright (C) 2007-2008,  Karlsruhe University
 *                
 * File path:     eacc_mgr.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#include <common/hthread.h>
#include <common/string.h>
#include <resourcemon/resourcemon.h>
#include <resourcemon/vm.h>
#include <resourcemon/earm.h>


hthread_t *eacc_mgr_thread;

resource_t resources[UUID_IEarm_AccResMax];
resource_buffer_t resource_buffer[UUID_IEarm_AccResMax];

L4_Word_t max_uuid_cpu;
L4_Word_t max_domain_in_use;

IDL4_INLINE void IEarm_register_resource_implementation(CORBA_Object _caller, const guid_t guid, idl4_fpage_t *fp, idl4_server_environment *_env)
{
    /* implementation of IAccounting::Manager::register_resource */
    L4_KDB_Enter("UNTESTED IEARM_REGISTER_RESOURCE");

    //printf( "EARM: register_resource thread " << _caller << " registers for guid " << guid << "\n");
    //L4_KDB_Enter("register_resource");
    
    // special handling for cpu (don't map, since we are in the same address space)
    if ( guid >= UUID_IEarm_AccResCPU_Min && guid <= UUID_IEarm_AccResCPU_Max ) {
	resources[guid].shared = (IEarm_shared_t *) &resource_buffer[guid];
	resources[guid].tid = _caller;
	return;
    }

    if( guid < UUID_IEarm_AccResMax ){
	L4_Word_t addr = 0;
	L4_Word_t haddr = (L4_Word_t) &resource_buffer[guid];
	L4_Word_t privileges = 7;

	idl4_fpage_set_base( fp, addr );
	idl4_fpage_set_page( fp, L4_FpageLog2( haddr, PAGE_BITS) );
	idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
	idl4_fpage_set_permissions( fp, privileges );

	resources[guid].shared = (IEarm_shared_t *) haddr;
	resources[guid].tid = _caller;
    } else
	CORBA_exception_set( _env, ex_IEarm_invalid_guid_format, NULL );
    return;
}

IDL4_PUBLISH_IEARM_REGISTER_RESOURCE(IEarm_register_resource_implementation);

IDL4_INLINE void IEarm_open_implementation(CORBA_Object _caller, const guid_t guid, L4_ThreadId_t *tid, idl4_server_environment *_env)
{
    L4_KDB_Enter("UNTESTED IEARM_OPEN");
    /* implementation of IAccounting::Manager::open */

    L4_Word_t domain = tid_space_t::tid_to_space_id(_caller) + VM_DOMAIN_OFFSET;

    printf( "EARM open: domain %x (tid %t) opens guid %d\n", domain, _caller, guid);
    //L4_KDB_Enter("open");

    if( (guid < UUID_IEarm_AccResMax) && (!L4_IsNilThread(resources[guid].tid)) ) {
      resources[guid].shared->clients[domain].limit = EARM_ACC_UNLIMITED;
      *tid = resources[guid].tid;
    } else 
      /* !!! setting exceptions does not work !!! */
      //CORBA_exception_set( _env, ex_IEarm_unknown_resource, NULL );
      L4_KDB_Enter("EARM open: unknown resource");

    return;
}

IDL4_PUBLISH_IEARM_OPEN(IEarm_open_implementation);

IDL4_INLINE void IEarm_close_implementation(CORBA_Object _caller, const guid_t guid, idl4_server_environment *_env)
{
    L4_KDB_Enter("UNTESTED IEARM_CLOSE");
    /* implementation of IAccounting::Manager::close */
  
    L4_Word_t domain = tid_space_t::tid_to_space_id(_caller) + VM_DOMAIN_OFFSET;

    printf( "EARM close: domain %x (tid %t) closes guid %d\n", domain, _caller, guid);
    L4_KDB_Enter("close");

    if( (guid < UUID_IEarm_AccResMax) && (!L4_IsNilThread(resources[guid].tid)) ) {
      resources[guid].shared->clients[domain].limit = 0;
    } else
      CORBA_exception_set( _env, ex_IEarm_unknown_resource, NULL );


    return;
}

IDL4_PUBLISH_IEARM_CLOSE(IEarm_close_implementation);

/* Interface IAccounting::Resource */

IDL4_INLINE void IEarm_get_counter_implementation(CORBA_Object _caller, const guid_t guid,
						  L4_Word_t *hi, L4_Word_t *lo, idl4_server_environment *_env)
{
    L4_KDB_Enter("UNIMPLEMENTED IEARM_GET_COUNTER");
    
#if 0   
    /* implementation of IAccounting::Resource::get_counter */
    L4_Word64_t counter = 0;
    L4_Word_t domain = tid_space_t::tid_to_space_id(_caller) + VM_DOMAIN_OFFSET;

    
    printf( "CPU get_counter dom %d : <%x:%x>\n", domain, *hi, *lo);
    L4_KDB_Enter("Resource_get_counter called");
    for (L4_Word_t cpu = 0; cpu <= max_uuid_cpu; cpu++) {
	check_energy_abs(cpu, domain);

    }

    for (L4_Word_t v = 0; v <= UUID_IEarm_AccResMax; v++) {
	counter += manager_cpu_shared[cpu]->clients[domain].base_cost[v];
	counter + manager_cpu_shared[cpu]->clients[domain].access_cost[v];
    }
        
    *hi = (counter >> 32);
    *lo = counter;

#endif


    return;
}

IDL4_PUBLISH_IEARM_GET_COUNTER(IEarm_get_counter_implementation);

static earm_set_t diff_set, old_set;

static bool eacc_mgr_debug_resource(L4_Word_t u, L4_Word_t ms)
{
    energy_t energy;

    bool printed_device = false;

#define PRINT_HEADER()				\
    if (!printed_device)			\
    {						\
	printf("r %lu ", u);			\
	printed_device = true;			\
    }						\
    else					\
	printf(" ");				\
    if (!printed_domain)			\
	printf("d %lu ", d);			\
    printed_domain = true;
    
    for (L4_Word_t d = EACC_DEBUG_MIN_DOMAIN; d <= max_domain_in_use; d ++) 
    {
	bool printed_domain = false;
	
	
	/* Idle */
	for (L4_Word_t v = 0; v < UUID_IEarm_AccResMax; v++)
	{

	    energy = resources[u].shared->clients[d].base_cost[v];
	    diff_set.res[u].clients[d].base_cost[v] = energy - old_set.res[u].clients[d].base_cost[v];
	    old_set.res[u].clients[d].base_cost[v] = energy;
	    diff_set.res[u].clients[d].base_cost[v] /= ms;
	    if (u == UUID_IEarm_AccResDisk)
		diff_set.res[u].clients[d].base_cost[v] /= 1000;
	    
	    if (diff_set.res[u].clients[d].base_cost[v])
	    {
		PRINT_HEADER();
		printf("i %u %4lu ", v, (L4_Word_t) diff_set.res[u].clients[d].base_cost[v]);
	    }
	}	    
	for (L4_Word_t v = 0; v < UUID_IEarm_AccResMax; v++)
	{
	    /* Access */
	    energy = resources[u].shared->clients[d].access_cost[v];
	    diff_set.res[u].clients[d].access_cost[v] = energy - old_set.res[u].clients[d].access_cost[v];
	    old_set.res[u].clients[d].access_cost[v] = energy;	
	    
	    diff_set.res[u].clients[d].access_cost[v] /= ms;
	    if (u == UUID_IEarm_AccResDisk)
		diff_set.res[u].clients[d].access_cost[v] /= 1000;
	    
	    if (diff_set.res[u].clients[d].access_cost[v])
	    {
		PRINT_HEADER();
		printed_domain = true;
		printf("a %u %5lu ", v, (L4_Word_t) diff_set.res[u].clients[d].access_cost[v]);
	    }

	}
	if (printed_domain)
	    printf(" ");
    }
    

    return printed_device;
   
}

void eacc_mgr_debug()
{
    
    L4_Clock_t now = L4_SystemClock();
    static L4_Clock_t last = { raw: 0 };
    L4_Word64_t ms = now.raw - last.raw;
    ms /= 1000;
    last = now;
    
    bool printed = false;
#if defined(EACC_DEBUG_CPU)
    for (L4_Word_t uuid_cpu = EACC_DEBUG_MIN_RESOURCE; uuid_cpu <= max_uuid_cpu; uuid_cpu++)
	printed |= eacc_mgr_debug_resource(uuid_cpu, ms);
    
#endif
#if defined(EACC_DEBUG_DISK)
    printed |= eacc_mgr_debug_resource(UUID_IEarm_AccResDisk, ms);
#endif
    if (printed)
	printf("\n");
    
}

void propagate_max_domain_in_use(L4_Word_t domain)
{
    for (L4_Word_t u = 0; u < UUID_IEarm_AccResMax; u++)
    {
	if (resources[u].tid != L4_nilthread)
	{
	    ASSERT(resources[u].shared);
	    if (resources[u].shared->max_domain_in_use < domain)
		resources[u].shared->max_domain_in_use = domain;
	}
    }
	
}


void eacc_mgr_init()
{
    for (L4_Word_t u = 0; u < UUID_IEarm_AccResMax; u++)
    {
        resources[u].tid = L4_nilthread;
	for (L4_Word_t i = 0; i < RESOURCE_BUFFER_SIZE / sizeof(L4_Word_t) ; i++)
	    resource_buffer[u][i] = 0;
    }
    
#if defined(EACC_DEBUG)
    for (L4_Word_t d = 0; d < L4_LOGGING_MAX_DOMAINS; d++)
	for (L4_Word_t u = 0; u < UUID_IEarm_AccResMax; u++)
	    for (L4_Word_t v = 0; v < UUID_IEarm_AccResMax; v++)
	    {
		old_set.res[u].clients[d].base_cost[v] = 
		    diff_set.res[u].clients[d].base_cost[v] = 0;
		old_set.res[u].clients[d].access_cost[v] = 
		    diff_set.res[u].clients[d].access_cost[v] = 0;
	    }
#endif
}

