/*********************************************************************
 *                
 * Copyright (C) 2006-2008,  Karlsruhe University
 *                
 * File path:     earm_acccpu.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#include <l4/types.h>
#include <l4/kip.h>

#include <common/hthread.h>
#include <resourcemon/resourcemon.h>
#include <resourcemon/vm.h>
#include <resourcemon/earm.h>

static L4_Word64_t  last_acc_timestamp[L4_LOGGING_MAX_CPUS][L4_LOGGING_MAX_DOMAINS];

IEarm_shared_t *manager_cpu_shared[UUID_IEarm_AccResCPU_Max + 1];

#define IDLE_ACCOUNT

L4_Word64_t debug_pmc[8];

// don't write to the log, because the guest also reads it
//#define LOG_READONLY

void check_energy_abs(L4_Word_t cpu, L4_Word_t domain)
{
    L4_Word64_t most_recent_timestamp = 0;
    L4_Word64_t exit_pmc[8], entry_pmc[8];
    energy_t idle_energy = 0, access_energy = 0, sum_idle_energy = 0, sum_access_energy = 0;
    L4_Word_t entry_event = 0, exit_event;
    L4_Word_t count = 0;
    /*
     * Get logfile
     */
    log_event_control_t *s = L4_RESOURCE_SELECTOR(cpu, domain, L4_RESOURCE_CPU);
    logfile_control_t *c = l4_logfile_base[cpu] + (s->logfile_selector / sizeof(logfile_control_t)) ;
    volatile L4_Word_t *current_idx = (L4_Word_t* ) (c + (c->X.current_offset / sizeof(logfile_control_t)));

    //printf( "selector " << (void*) s
    // << " ctrl reg " << (void*) c
    // << " current_idx " << (void*) current_idx
    // << " = " <<  *current_idx
    // << "\n");

    if (L4_LOGGING_LOG_ENTRIES(c)==0) {
	printf( "No log entries found for CPU " << cpu << "! Break!\n");
	return;	    
    }

    if (L4_LOGGING_LOG_ENTRIES(c)>2048) {
	printf( "Too many log entries (" << L4_LOGGING_LOG_ENTRIES(c) << ") found! Log may be in corrupt state. Abort!\n");
	return;
    }

    while (count <= (L4_LOGGING_LOG_ENTRIES(c)-32))
    {
	
	/*
	 * Logfile of non-running domain contains 8 entry-exit pairs 
	 * read all 8 exit pairs
	 */
	for (L4_Word_t ctr=0; ctr < 8; ctr++)
	{
	    L4_LOGGING_DEC_LOG(current_idx, c);
	    exit_pmc[7-ctr] = *current_idx;
	    exit_pmc[7-ctr] <<= 32;
	    L4_LOGGING_DEC_LOG(current_idx, c);
	    exit_pmc[7-ctr] += *current_idx;
	}

	exit_event = (L4_Word_t) (exit_pmc[0] & 0x1);; 
	
	if (exit_event != L4_RESOURCE_CPU)
	{
	    {
		if (exit_event == L4_RESOURCE_CPU_ENTRY)
		{
		    count+=16;
		    continue;
		}
		    
		printf( "Logfile mismatch exit " 
		     << " evt " << exit_event << "/" << L4_RESOURCE_CPU
		     << " dom " << domain
		     << " idx " << (void*) current_idx 
		     << " ct  " << count
		     << " sz  " << L4_LOGGING_LOG_ENTRIES(c)
		     << "\n");
		//L4_KDB_Enter("BUG");
	    }
	}
	
	
	if (most_recent_timestamp == 0)
	{
	    //printf( "timestamp " << (void *) (L4_Word_t) exit_pmc[0] << "\n");
	    most_recent_timestamp = exit_pmc[0];;
	}

	/*
	 * Reached stale entries ?
	 */
	if (exit_pmc[0] <= last_acc_timestamp[cpu][domain])
	    break;
	
	/*
	 * read all 8 entry pairs 
	 */
	for (L4_Word_t ctr=0; ctr < 8; ctr++)
	{
	    L4_LOGGING_DEC_LOG(current_idx, c);
	    entry_pmc[7-ctr] = *current_idx;
	    entry_pmc[7-ctr] <<= 32;
	    L4_LOGGING_DEC_LOG(current_idx, c);
	    entry_pmc[7-ctr] += *current_idx;
	}
	
	entry_event = (L4_Word_t) (entry_pmc[0] & 0x1);; 
	
	if (entry_event != L4_RESOURCE_CPU_ENTRY)
	{
	    printf( "Logfile mismatch entry " 
		 << " evt " << entry_event << "/" << L4_RESOURCE_CPU_ENTRY
		 << " dom " << domain
		 << " idx " << (void *) current_idx
		 << " ct  " << count
		 << " sz  " << L4_LOGGING_LOG_ENTRIES(c)
		 << "\n");
	    L4_KDB_Enter("BUG");
	}

	/* Wraparound ? */
	if (exit_pmc[0] <= entry_pmc[0])
	{
	    printf( "wraparound ct " << count << "\n");
	    //L4_KDB_Enter("wraparound catcher");
	    break;
	}
	/*
	 * Estimate energy from counters 
	 */
#define DIVISOR 100
	idle_energy = 
	    pmc_weight[0]  * ((exit_pmc[0] - entry_pmc[0])/DIVISOR); // tsc

	access_energy = 0;
	for (L4_Word_t pmc=1; pmc < 8; pmc++)
	    access_energy +=  pmc_weight[pmc] * ((exit_pmc[pmc] - entry_pmc[pmc])/DIVISOR);

	for (L4_Word_t pmc=0; pmc < 8; pmc++)
	    debug_pmc[pmc] +=  ((exit_pmc[pmc] - entry_pmc[pmc])/DIVISOR);


#if defined(IDLE_ACCOUNT)
	sum_idle_energy += idle_energy;
#else
	sum_access_energy += idle_energy;	
	sum_idle_energy = 0;
#endif	
       sum_access_energy += access_energy;
       
       count += 32;
    }
    
    if (vm_t::max_domain_in_use > EARM_ACC_MIN_DOMAIN)
	sum_idle_energy /= (vm_t::max_domain_in_use - EARM_ACC_MIN_DOMAIN + 1);
    
    sum_access_energy /= 1000;
    sum_idle_energy /= 1000;
    
    last_acc_timestamp[cpu][domain] = most_recent_timestamp;
    manager_cpu_shared[cpu]->clients[domain].access_cost[cpu] += sum_access_energy;
   
    for (L4_Word_t d = EARM_ACC_MIN_DOMAIN; d <= vm_t::max_domain_in_use; d++) {
	//manager_cpu_shared[cpu]->clients[d].base_cost += 1000;
       manager_cpu_shared[cpu]->clients[d].base_cost[cpu] += sum_idle_energy;
    }

    //L4_KDB_Enter("Checked energy");

}


/*****************************************************************
 * Module IAccounting
 *****************************************************************/

/* Interface IAccounting::Resource */

IDL4_INLINE void IEarm_AccResource_get_counter_implementation(CORBA_Object _caller, L4_Word_t *hi, L4_Word_t *lo,
							      idl4_server_environment *_env)
{
    /* implementation of IAccounting::Resource::get_counter */
    L4_Word64_t counter = 0;
    L4_Word_t domain = tid_space_t::tid_to_space_id(_caller) + VM_DOMAIN_OFFSET;

    printf( "CPU get_counter " << domain << ": " << *hi << " / " << *lo << "\n");
    L4_KDB_Enter("Resource_get_counter called");
#if 0   
    for (L4_Word_t cpu = 0; cpu <= max_uuid_cpu; cpu++) {
	check_energy_abs(cpu, domain);

    }

    for (L4_Word_t v = 0; v <= UUID_IEarm_AccResMax; v++) {
	counter += manager_cpu_shared[cpu]->clients[domain].base_cost[v];
	counter + manager_cpu_shared[cpu]->clients[domain].access_cost[v];
    }
#endif
    
    *hi = (counter >> 32);
    *lo = counter;


    return;
}
IDL4_PUBLISH_IEARM_ACCRESOURCE_GET_COUNTER(IEarm_AccResource_get_counter_implementation);

void *IEarm_AccResource_vtable[IEARM_ACCRESOURCE_DEFAULT_VTABLE_SIZE] = IEARM_ACCRESOURCE_DEFAULT_VTABLE;


void IEarm_AccResource_server(
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
  L4_ThreadId_t partner;
  L4_MsgTag_t msgtag;
  idl4_msgbuf_t msgbuf;
  long cnt;

  
  max_uuid_cpu = L4_NumProcessors(L4_GetKernelInterface()) - 1;
  if (max_uuid_cpu > UUID_IEarm_AccResCPU_Max)
      max_uuid_cpu = UUID_IEarm_AccResCPU_Max;

  for (L4_Word_t uuid_cpu = 0; uuid_cpu <= max_uuid_cpu; uuid_cpu++)
  {
      /* register with the resource manager */
      earm_acccpu_register( L4_Myself(), uuid_cpu, &manager_cpu_shared[uuid_cpu] );
  }
  idl4_msgbuf_init(&msgbuf);
//  for (cnt = 0;cnt < IEARM_ACCRESOURCE_STRBUF_SIZE;cnt++)
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

          idl4_process_request(&partner, &msgtag, 
			       &msgbuf, &cnt, 
			       IEarm_AccResource_vtable[idl4_get_function_id(&msgtag) & IEARM_ACCRESOURCE_FID_MASK]);
        }
    }
}

void IEarm_AccResource_discard()
{
}


void earm_acccpu_collect()
{
    for (L4_Word_t cpu = 0; cpu <= max_uuid_cpu; cpu++) {
	for (L4_Word_t domain = EARM_ACC_MIN_DOMAIN; domain <= vm_t::max_domain_in_use; domain++)
	{
		//printf( "Collector checking CPU energy cpu " << cpu << " domain " << domain << "\n");
	    check_energy_abs(cpu, domain);
	}
    }
}


#if !defined(CONFIG_X_EARM_EAS)
static void earm_acccpu_collector( 
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    L4_Time_t sleep = L4_TimePeriod( EARM_ACCCPU_MSEC * 1000 );

    while (1) {
	earm_acccpu_collect();
	L4_Sleep(sleep);
    }
}
#endif
void earm_acccpu_init()
{
    /* Start resource thread */
    hthread_t *earm_acccpu_thread = get_hthread_manager()->create_thread( 
	hthread_idx_earm_acccpu, 252,
	IEarm_AccResource_server);

    if( !earm_acccpu_thread )
    {
	printf( "EARM: couldn't start cpu accounting manager" ;
	L4_KDB_Enter();
	return;
    }
    printf( "EARM: cpu accounting manager TID: " << earm_acccpu_thread->get_global_tid() << '\n';

    earm_acccpu_thread->start();


#if !defined(CONFIG_X_EARM_EAS)
    /* Start collector thread */
    hthread_t *earm_acccpu_col_thread = get_hthread_manager()->create_thread( 
	hthread_idx_earm_acccpu_col, 252,
	earm_acccpu_collector);

    if( !earm_acccpu_col_thread )
    {
	printf( "EARM: couldn't start CPU accounting collector thread" ;
	L4_KDB_Enter();
	return;
    }
    printf( "EARM: CPU accounting collector TID: " << earm_acccpu_col_thread->get_global_tid() << '\n';
    
    earm_acccpu_col_thread->start();
#endif
}
