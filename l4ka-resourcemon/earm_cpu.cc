/*********************************************************************
 *                
 * Copyright (C) 2006-2007, 2009-2010,  Karlsruhe University
 *                
 * File path:     earm_cpu.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include "earm.h"

static L4_Word64_t  last_acc_timestamp[IResourcemon_max_cpus][L4_LOG_MAX_LOGIDS];

IEarm_shared_t *manager_cpu_shared[UUID_IEarm_ResCPU_Max + 1];

#define IDLE_ACCOUNT

L4_Word64_t debug_pmc[8];

// don't write to the log, because the guest also reads it
//#define LOG_READONLY

#define DUMP_LOGFILE()                                                  \
    printf("cpu %d logid %d base %p sel base %p ofs %p ctrl reg %x ofs %p cidx %p\n", \
           cpu, logid, l4_logfile_base[cpu], l4_logselector_base[cpu],  \
           l4_logselector_base[cpu][(logid * L4_LOG_MAX_RESOURCES) + L4_LOG_RESOURCE_PMC], c, \
           ((L4_Word_t) current_idx - (L4_Word_t) c), current_idx)

#define DUMP_COUNTERS()                                                 \
    for (L4_Word_t ctr=0; ctr < 8; ctr++)                               \
    {                                                                   \
        printf("pmc %d\texit %8x %8x \t- entry %8x / %8x = delta %8x\n", ctr, \
               (L4_Word_t) (exit_pmc[ctr] >> 32), (L4_Word_t) exit_pmc[ctr], \
               (L4_Word_t) (entry_pmc[ctr] >> 32), (L4_Word_t) entry_pmc[ctr], \
               (L4_Word_t) (exit_pmc[ctr] - entry_pmc[ctr]));           \
    }

void earmcpu_update(L4_Word_t cpu, L4_Word_t logid)
{
    L4_Word64_t most_recent_timestamp = 0;
    L4_Word64_t exit_pmc[8], entry_pmc[8];
    energy_t idle_energy = 0, access_energy = 0, sum_idle_energy = 0, sum_access_energy = 0;
    L4_Word_t entry_event = 0, exit_event;
    L4_Word_t count = 0;
    /*
     * Get logfile
     */
    
    L4_LogCtrl_t *c = L4_LogFile(l4_logfile_base[cpu], l4_logselector_base[cpu], logid, L4_LOG_RESOURCE_PMC);
    volatile L4_Word_t *current_idx = (L4_Word_t* ) (c + (c->X.current_offset / sizeof(L4_LogCtrl_t)));

   
    if (L4_LOG_ENTRIES(c)==0) {
	printf("No log entries found for CPU %d, skip", cpu);
	return;	    
    }

    if (L4_LOG_ENTRIES(c)>2048) 
    {
	printf("Too many log entries (%d) found. Log may be in corrupt state. Skip\n",  L4_LOG_ENTRIES(c));
	return;
    }

    while (count <= (L4_LOG_ENTRIES(c)-32))
    {
	
	/*
	 * Logfile of non-running logid contains 8 entry-exit pairs 
         * most recent one must be an entry event
	 */
	for (L4_Word_t ctr=0; ctr < 8; ctr++)
	{
            L4_LOG_DEC_LOG(current_idx, c);
	    exit_pmc[7-ctr] = *current_idx;
	    exit_pmc[7-ctr] <<= 32;
	    L4_LOG_DEC_LOG(current_idx, c);
	    exit_pmc[7-ctr] += *current_idx;
	}

	exit_event = (L4_Word_t) (exit_pmc[0] & 0x1);
	
        /* Entry event first -- skip */
        if (exit_event == 1)
        {
            count+=16;
            continue;
        }

	if (most_recent_timestamp == 0)
	{
	    //hout << "timestamp " << (void *) (L4_Word_t) exit_pmc[0] << "\n";
	    most_recent_timestamp = exit_pmc[0];;
	}

	/*
	 * Reached stale entries ?
	 */
	if (exit_pmc[0] <= last_acc_timestamp[cpu][logid])
	    break;
	
	/*
	 * read all 8 entry pairs 
	 */
	for (L4_Word_t ctr=0; ctr < 8; ctr++)
	{
	    L4_LOG_DEC_LOG(current_idx, c);
	    entry_pmc[7-ctr] = *current_idx;
	    entry_pmc[7-ctr] <<= 32;
	    L4_LOG_DEC_LOG(current_idx, c);
	    entry_pmc[7-ctr] += *current_idx;
	}

	entry_event = (L4_Word_t) (entry_pmc[0] & 0x1);
        
        /* 1 == entry */
	if (entry_event != 1)
	{
            /* log start */
            if (entry_pmc[0] == 0)
                break;
            
            printf("Logfile mismatch entry evt %d/1 logid %d idx %x ct %d sz %d",
                   entry_event, logid, current_idx, count, L4_LOG_ENTRIES(c));

            DUMP_LOGFILE();
            DUMP_COUNTERS();
            
            L4_KDB_Enter("Mismatch");
	}
        
        /* Wraparound ? */
	if (exit_pmc[0] <= entry_pmc[0])
	{
	    printf("wraparound count %d\n",count);
	    break;
	}

        //DUMP_LOGFILE();
        //DUMP_COUNTERS();
        //L4_KDB_Enter("Next PMC");
	/*
	 * Estimate energy from counters 
	 */
	idle_energy = 
	    pmc_weight[0]  * ((exit_pmc[0] - entry_pmc[0])/EARM_CPU_DIVISOR); // tsc

	access_energy = 0;
	for (L4_Word_t pmc=1; pmc < 8; pmc++)
            access_energy += pmc_weight[pmc] * ((exit_pmc[pmc] - entry_pmc[pmc])/EARM_CPU_DIVISOR);
        

#if defined(IDLE_ACCOUNT)
	sum_idle_energy += idle_energy;
#else
	sum_access_energy += idle_energy;	
	sum_idle_energy = 0;
#endif	
        sum_access_energy += access_energy;
       
        count += 32;
    }
    
    if (max_logid_in_use > EARM_MIN_LOGID)
	sum_idle_energy /= (max_logid_in_use - EARM_MIN_LOGID + 1);
    
    sum_access_energy /= 1000;
    sum_idle_energy /= 1000;
    
    last_acc_timestamp[cpu][logid] = most_recent_timestamp;
    manager_cpu_shared[cpu]->clients[logid].access_cost[cpu] += sum_access_energy;
   
    for (L4_Word_t d = EARM_MIN_LOGID; d <= max_logid_in_use; d++) {
        manager_cpu_shared[cpu]->clients[d].base_cost[cpu] += sum_idle_energy;
    }

    //L4_KDB_Enter("Checked energy");

}

void earmcpu_pmc_snapshot(L4_Word64_t *pmcstate)
{
    
    /* PMCRegs: TSC, UC, MQW, RB, MB, MR, MLR, LDM */
    pmcstate[0] = x86_rdtsc();
    pmcstate[1] = x86_rdpmc(0);
    pmcstate[2] = x86_rdpmc(4);
    pmcstate[3] = x86_rdpmc(5);
    pmcstate[4] = x86_rdpmc(12);
    pmcstate[5] = x86_rdpmc(13);
    pmcstate[6] = x86_rdpmc(1);
    pmcstate[7] = x86_rdpmc(14);

}

void earmcpu_update(L4_Word_t cpu, L4_Word_t logid, L4_Word64_t *pmcstate, L4_Word64_t *lpmcstate, L4_Word64_t *energy, L4_Word64_t *tsc)
{
  
    energy_t idle_energy = 0, access_energy = 0;
    L4_Word64_t old_pmc, new_pmc, diff_pmc;    
    

    /* IDLE Energy */
    if (pmcstate[0] < lpmcstate[0])
    {
	old_pmc = 0;
	new_pmc = pmcstate[0] + (~0UL - lpmcstate[0]);
    }
    else
    {
	old_pmc = lpmcstate[0];
	new_pmc = pmcstate[0];
    }
    
    diff_pmc = (new_pmc - old_pmc);
    *tsc = diff_pmc;
    
    //dprintf(debug_virq, "VIRQ update energy logid %d max %d\n", logid, max_logid_in_use);
    //dprintf(debug_virq, "\tsc new %x %x old %x %x diff %d\n", 
    //    (L4_Word_t) (new_pmc >> 32), (L4_Word_t) new_pmc, 
    //    (L4_Word_t) (old_pmc >> 32), (L4_Word_t) old_pmc,
    //    (L4_Word_t) diff_pmc);

    idle_energy = pmc_weight[0] * diff_pmc;
	    
    for (L4_Word_t pmc=1; pmc < 8; pmc++)
    {    
	if (pmcstate[pmc] < lpmcstate[pmc])
	{
	    old_pmc = 0;
	    new_pmc = pmcstate[pmc] + (~0UL - lpmcstate[pmc]);
	}
	else
	{
	    old_pmc = lpmcstate[pmc];
	    new_pmc = pmcstate[pmc];
	}
	
	diff_pmc = (new_pmc - old_pmc);
	access_energy +=  pmc_weight[pmc]  * diff_pmc;
	
	//dprintf(debug_virq, "\tpmc %d new %x %x old %x %x diff %d\n", 
	//pmc,  
	//(L4_Word_t) (new_pmc >> 32), (L4_Word_t) new_pmc, 
	//(L4_Word_t) (old_pmc >> 32), (L4_Word_t) old_pmc,
	//(L4_Word_t) diff_pmc);
    }
    
    access_energy /= 1000 * EARM_CPU_DIVISOR;
    idle_energy   /= 1000 * EARM_CPU_DIVISOR;
    
    dprintf(debug_virq+1, "VIRQ %d account energy logid %d access %d idle %d pass %d\n", 
	    cpu, logid, (L4_Word_t) access_energy, (L4_Word_t) idle_energy);

    for (L4_Word_t d = EARM_MIN_LOGID; d <= max_logid_in_use; d++) 
	resources[cpu].shared->clients[d].base_cost[cpu] += idle_energy / (max_logid_in_use + 1);
    
    manager_cpu_shared[cpu]->clients[logid].access_cost[cpu] += access_energy;
    
    //return (idle_energy + access_energy);
    *energy = access_energy;
}



/*****************************************************************
 * Module Iounting
 *****************************************************************/

/* Interface Iounting::Resource */
IDL4_INLINE void  IEarm_Resource_get_client_info_implementation(CORBA_Object  _caller, 
                                                                const L4_Word_t  client_space, 
                                                                idl4_fpage_t * client_config, 
                                                                idl4_fpage_t * server_config, 
                                                                idl4_server_environment * _env)
{
    printf("EARM CPU: get client info %t\n", _caller);
    UNIMPLEMENTED();
}

IDL4_PUBLISH_IEARM_RESOURCE_GET_CLIENT_INFO(IEarm_Resource_get_client_info_implementation);


IDL4_INLINE void IEarm_Resource_get_counter_implementation(CORBA_Object _caller, L4_Word_t *hi, L4_Word_t *lo,
							   idl4_server_environment *_env)
{
    /* implementation of Iounting::Resource::get_counter */
    L4_Word64_t counter = 0;

    printf("EARM CPU: get_counter %x\n", _caller);
    L4_KDB_Enter("Resource_get_counter called");
#if 0   
    L4_Word_t logid = tid_space_t::tid_to_space_id(_caller) + VM_LOGID_OFFSET;

    for (L4_Word_t cpu = 0; cpu <= max_uuid_cpu; cpu++) {
	earmcpu_update(cpu, logid);

    }

    for (L4_Word_t v = 0; v <= UUID_IEarm_ResMax; v++) {
	counter += manager_cpu_shared[cpu]->clients[logid].base_cost[v];
	counter + manager_cpu_shared[cpu]->clients[logid].access_cost[v];
    }
#endif
    
    *hi = (counter >> 32);
    *lo = counter;


    return;
}
IDL4_PUBLISH_IEARM_RESOURCE_GET_COUNTER(IEarm_Resource_get_counter_implementation);

void *IEarm_Resource_vtable[IEARM_RESOURCE_DEFAULT_VTABLE_SIZE] = IEARM_RESOURCE_DEFAULT_VTABLE;


void IEarm_Resource_server(
    void *param UNUSED,
    hthread_t *htread UNUSED)
{
  L4_ThreadId_t partner;
  L4_MsgTag_t msgtag;
  idl4_msgbuf_t msgbuf;
  long cnt;

  
  max_uuid_cpu = L4_NumProcessors(l4_kip) - 1;
  if (max_uuid_cpu > UUID_IEarm_ResCPU_Max)
      max_uuid_cpu = UUID_IEarm_ResCPU_Max;

  for (L4_Word_t uuid_cpu = 0; uuid_cpu <= max_uuid_cpu; uuid_cpu++)
  {
      /* register with the resource manager */
      earmcpu_register( L4_Myself(), uuid_cpu, &manager_cpu_shared[uuid_cpu] );
  }
  idl4_msgbuf_init(&msgbuf);
//  for (cnt = 0;cnt < IEARM_RESOURCE_STRBUF_SIZE;cnt++)
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
			       IEarm_Resource_vtable[idl4_get_function_id(&msgtag) & IEARM_RESOURCE_FID_MASK]);
        }
    }
}

void IEarm_Resource_discard()
{
}


void earmcpu_collect()
{
    for (L4_Word_t cpu = 0; cpu <= max_uuid_cpu; cpu++) {
	for (L4_Word_t logid = EARM_MIN_LOGID; logid <= max_logid_in_use; logid++)
	{
            //hout << "Collector checking CPU energy cpu " << cpu << " logid " << logid << "\n";
	    earmcpu_update(cpu, logid);
	}
    }
}



void earmcpu_init()
{
    /* Start resource thread */
    hthread_t *earmcpu_thread = get_hthread_manager()->create_thread( 
	hthread_idx_earmcpu, 252, false,
	IEarm_Resource_server);

    if( !earmcpu_thread )
    {
	printf("\tEARM couldn't start cpu accounting manager");
	L4_KDB_Enter();
	return;
    }
    printf("\tEARM CPU accounting manager TID %t\n", earmcpu_thread->get_global_tid());

    earmcpu_thread->start();


}
 
