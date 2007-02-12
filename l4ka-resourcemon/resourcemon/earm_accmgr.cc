/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     earm_accmgr.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#include <common/hconsole.h>
#include <common/console.h>
#include <common/hthread.h>
#include <common/string.h>
#include <resourcemon/resourcemon.h>
#include <resourcemon/vm.h>
#include <resourcemon/earm.h>
#include "earm_idl_client.h"


hthread_t *earm_accmanager_thread;

resource_t resources[UUID_IEarm_AccResMax];

#define LOGFILE_SIZE (1 << PAGE_BITS)
typedef L4_Word8_t res_logfile_t[LOGFILE_SIZE] __attribute__ ((aligned (LOGFILE_SIZE)));
res_logfile_t res_logfile[UUID_IEarm_AccResMax];

L4_Word_t max_uuid_cpu;

IDL4_INLINE void IEarm_AccManager_register_resource_implementation(CORBA_Object _caller, const guid_t guid, idl4_fpage_t *fp, idl4_server_environment *_env)
{
    /* implementation of IAccounting::Manager::register_resource */

    hout << "EARM: register_resource thread " << _caller << " registers for guid " << guid << "\n";
    //L4_KDB_Enter("register_resource");
    
    // special handling for cpu (don't map, since we are in the same address space)
    if ( guid >= UUID_IEarm_AccResCPU_Min && guid <= UUID_IEarm_AccResCPU_Max ) {
	resources[guid].shared = (IEarm_shared_t *) &res_logfile[guid];
	resources[guid].tid = _caller;
	return;
    }

    if( guid < UUID_IEarm_AccResMax ){
	L4_Word_t addr = 0;
	L4_Word_t haddr = (L4_Word_t) &res_logfile[guid];
	L4_Word_t privileges = 7;

	idl4_fpage_set_base( fp, addr );
	idl4_fpage_set_page( fp, L4_FpageLog2( haddr, PAGE_BITS) );
	idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
	idl4_fpage_set_permissions( fp, privileges );

	resources[guid].shared = (IEarm_shared_t *) haddr;
	resources[guid].tid = _caller;
    } else
	CORBA_exception_set( _env, ex_IEarm_AccManager_invalid_guid_format, NULL );
    return;
}

IDL4_PUBLISH_IEARM_ACCMANAGER_REGISTER_RESOURCE(IEarm_AccManager_register_resource_implementation);

IDL4_INLINE void IEarm_AccManager_open_implementation(CORBA_Object _caller, const guid_t guid, L4_ThreadId_t *tid, idl4_server_environment *_env)
{
    /* implementation of IAccounting::Manager::open */

    L4_Word_t domain = tid_space_t::tid_to_space_id(_caller) + VM_DOMAIN_OFFSET;

    hout << "EARM open: domain " << domain << " (thread " << _caller << ") opens guid " << guid << "\n";
    //L4_KDB_Enter("open");

    if( (guid < UUID_IEarm_AccResMax) && (!L4_IsNilThread(resources[guid].tid)) ) {
      resources[guid].shared->clients[domain].limit = EARM_ACC_UNLIMITED;
      *tid = resources[guid].tid;
    } else 
      /* !!! setting exceptions does not work !!! */
      //CORBA_exception_set( _env, ex_IEarm_AccManager_unknown_resource, NULL );
      L4_KDB_Enter("EARM open: unknown resource");

    return;
}

IDL4_PUBLISH_IEARM_ACCMANAGER_OPEN(IEarm_AccManager_open_implementation);

IDL4_INLINE void IEarm_AccManager_close_implementation(CORBA_Object _caller, const guid_t guid, idl4_server_environment *_env)
{
    /* implementation of IAccounting::Manager::close */
  
    L4_Word_t domain = tid_space_t::tid_to_space_id(_caller) + VM_DOMAIN_OFFSET;

    hout << "EARM close: domain " << domain << " (thread " << _caller << ") closes guid " << guid << "\n";
    L4_KDB_Enter("close");

    if( (guid < UUID_IEarm_AccResMax) && (!L4_IsNilThread(resources[guid].tid)) ) {
      resources[guid].shared->clients[domain].limit = 0;
    } else
      CORBA_exception_set( _env, ex_IEarm_AccManager_unknown_resource, NULL );


    return;
}

IDL4_PUBLISH_IEARM_ACCMANAGER_CLOSE(IEarm_AccManager_close_implementation);

void *IEarm_AccManager_vtable[IEARM_ACCMANAGER_DEFAULT_VTABLE_SIZE] = IEARM_ACCMANAGER_DEFAULT_VTABLE;

void IEarm_AccManager_server(
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
  L4_ThreadId_t partner;
  L4_MsgTag_t msgtag;
  idl4_msgbuf_t msgbuf;
  long cnt;

  /* register with the locator */
  hout << "EARM: accounting manager register " << UUID_IEarm_AccManager << "\n";
  register_interface( UUID_IEarm_AccManager, L4_Myself() );

  idl4_msgbuf_init(&msgbuf);
//  for (cnt = 0;cnt < IEARM_ACCMANAGER_STRBUF_SIZE;cnt++)
//    idl4_msgbuf_add_buffer(&msgbuf, malloc(8000), 8000);

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

          idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, IEarm_AccManager_vtable[idl4_get_function_id(&msgtag) & IEARM_ACCMANAGER_FID_MASK]);
        }
    }
}

void IEarm_AccManager_discard(void)
{
}

    
static earm_set_t diff_set, old_set;

static bool earm_accmanager_debug_resource(L4_Word_t u, L4_Word_t ms)
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
    

    for (L4_Word_t d = EARM_ACC_DEBUG_MIN_DOMAIN; d <= vm_t::max_domain_in_use; d ++) 
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

static void earm_accmanager_debug(
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    L4_Time_t sleep = L4_TimePeriod( EARM_ACC_DEBUG_MSEC * 1000 );

    for (L4_Word_t d = 0; d < L4_LOGGING_MAX_DOMAINS; d++)
	for (L4_Word_t u = 0; u < UUID_IEarm_AccResMax; u++)
	    for (L4_Word_t v = 0; v < UUID_IEarm_AccResMax; v++)
	    {
		old_set.res[u].clients[d].base_cost[v] = 
		    diff_set.res[u].clients[d].base_cost[v] = 0;
		old_set.res[u].clients[d].access_cost[v] = 
		    diff_set.res[u].clients[d].access_cost[v] = 0;
	    }
    
    while (1) {

	L4_Clock_t now = L4_SystemClock();
	static L4_Clock_t last = { raw: 0 };
	L4_Word64_t ms = now.raw - last.raw;
	ms /= 1000;
	last = now;

#if defined(EARM_ACC_DEBUG_CPU)
	bool printed = false;
	for (L4_Word_t uuid_cpu = EARM_ACC_DEBUG_MIN_RESOURCE; uuid_cpu <= max_uuid_cpu; uuid_cpu++)
	    printed |= earm_accmanager_debug_resource(uuid_cpu, ms);
	
#endif
#if defined(EARM_ACC_DEBUG_DISK)
	printed |= earm_accmanager_debug_resource(UUID_IEarm_AccResDisk, ms);
#endif
	if (printed)
	    printf("\n");
	
	L4_Sleep(sleep);
    }
}

void earm_accmanager_init()
{
    
    
    for (L4_Word_t u = 0; u < UUID_IEarm_AccResMax; u++)
    {
        resources[u].tid = L4_nilthread;
	for (L4_Word_t i = 0; i < LOGFILE_SIZE / sizeof(L4_Word_t) ; i++)
	    res_logfile[u][i] = 0;
    }

    /* Start resource manager thread */
    earm_accmanager_thread = get_hthread_manager()->create_thread( 
	hthread_idx_earm_accmanager, 252,
	IEarm_AccManager_server);

    if( !earm_accmanager_thread )
    {
	hout << "EARM: couldn't start accounting manager" ;
	L4_KDB_Enter();
	return;
    }
    hout << "EARM: accounting manager TID: " << earm_accmanager_thread->get_global_tid() << '\n';

    earm_accmanager_thread->start();

#if defined(EARM_ACC_DEBUG)
    /* Start debugger */
    hthread_t *earm_accmanager_debug_thread = get_hthread_manager()->create_thread( 
	hthread_idx_earm_accmanager_debug, 252,
	earm_accmanager_debug);

    if( !earm_accmanager_debug_thread )
    {
	hout << "EARM: couldn't accounting manager debugger" ;
	L4_KDB_Enter();
	return;
    }
    hout << "EARM: accounting manager debugger TID: " << earm_accmanager_debug_thread->get_global_tid() << '\n';

    earm_accmanager_debug_thread->start();
#endif

   

}

void earm_acccpu_register( L4_ThreadId_t tid, L4_Word_t uuid_cpu, IEarm_shared_t **shared )
{
  CORBA_Environment env = idl4_default_environment;
  IEarm_AccManager_register_resource( earm_accmanager_thread->get_global_tid(), uuid_cpu, 0, &env );
  //ASSERT( env._major != CORBA_USER_EXCEPTION );
  
  *shared = resources[uuid_cpu].shared;
  hout << "EARM: register cpu shared: " << *shared 
       << " resources[" << uuid_cpu << "].shared: " 
       << resources[uuid_cpu].shared << "\n";
}

