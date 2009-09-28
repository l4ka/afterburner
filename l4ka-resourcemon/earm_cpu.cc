/*********************************************************************
 *                
 * Copyright (C) 2006-2007, 2009,  Karlsruhe University
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
#include "virq.h"

static L4_Word64_t  last_acc_timestamp[IResourcemon_max_cpus][L4_LOG_MAX_LOGIDS];

IEarm_shared_t *manager_cpu_shared[UUID_IEarm_ResCPU_Max + 1];

#define IDLE_ACCOUNT
#define PMC_DIVISOR 100

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
        printf("pmc %d\texit %x %x \t- entry %x / %x = delta %x\n", ctr, \
               (exit_pmc[ctr] >> 32), (L4_Word_t) exit_pmc[ctr],        \
               (entry_pmc[ctr] >> 32), (L4_Word_t) entry_pmc[ctr],      \
               (L4_Word_t) (exit_pmc[ctr] - entry_pmc[ctr]));           \
    }

void check_energy_abs(L4_Word_t cpu, L4_Word_t logid)
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
	printf("No log entries found for CPU %d, skip");
	return;	    
    }

    if (L4_LOG_ENTRIES(c)>2048) 
    {
	printf("Too many log entries (%) found. Log may be in corrupt state. Skip\n",  L4_LOG_ENTRIES(c));
	return;
    }

    while (count <= (L4_LOG_ENTRIES(c)-32))
    {
	
	/*
	 * Logfile of non-running logid contains 8 entry-exit pairs 
	 * read all 8 exit pairs
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
	    pmc_weight[0]  * ((exit_pmc[0] - entry_pmc[0])/PMC_DIVISOR); // tsc

	access_energy = 0;
	for (L4_Word_t pmc=1; pmc < 8; pmc++)
            access_energy += pmc_weight[pmc] * ((exit_pmc[pmc] - entry_pmc[pmc])/PMC_DIVISOR);
        
	for (L4_Word_t pmc=0; pmc < 8; pmc++)
	    debug_pmc[pmc] +=  ((exit_pmc[pmc] - entry_pmc[pmc])/PMC_DIVISOR);


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

#if 0
void earm_cpu_pmc_snapshot(L4_IA32_PMCCtrlXferItem_t *pmcstate)
{
    
    /* PMCRegs: TSC, UC, MQW, RB, MB, MR, MLR, LDM */
    pmcstate->regs.reg64[0] = x86_rdtsc();
    pmcstate->regs.reg64[1] = x86_rdpmc(0);
    pmcstate->regs.reg64[2] = x86_rdpmc(4);
    pmcstate->regs.reg64[3] = x86_rdpmc(5);
    pmcstate->regs.reg64[4] = x86_rdpmc(12);
    pmcstate->regs.reg64[5] = x86_rdpmc(13);
    pmcstate->regs.reg64[6] = x86_rdpmc(1);
    pmcstate->regs.reg64[7] = x86_rdpmc(14);

}
#endif

void earm_cpu_update_records(word_t cpu, vm_context_t *vctx, L4_IA32_PMCCtrlXferItem_t *pmcstate)
{
  
    energy_t idle_energy = 0, access_energy = 0;
    energy_t diff_pmc;
    L4_Word64_t old_pmc, new_pmc;    
    word_t logid = vctx->logid;
    

    /* IDLE Energy */
    if (pmcstate->regs.reg[0] < vctx->last_pmc.regs.reg[0])
    {
	old_pmc = 0;
	new_pmc = pmcstate->regs.reg[0] + (~0UL - vctx->last_pmc.regs.reg[0]);
    }
    else
    {
	old_pmc = vctx->last_pmc.regs.reg[0];
	new_pmc = pmcstate->regs.reg[0];
    }
   
    diff_pmc = (new_pmc - old_pmc);
    
    dprintf(debug_virq, "VIRQ update energy logid %d\n\ttsc new %x %x old %x %x diff %d\n", 
	    logid, 
	    (L4_Word_t) (new_pmc >> 32), (L4_Word_t) new_pmc, 
	    (L4_Word_t) (old_pmc >> 32), (L4_Word_t) old_pmc,
	    (L4_Word_t) diff_pmc);

    idle_energy = pmc_weight[0] * diff_pmc;
	    
    for (L4_Word_t pmc=1; pmc < L4_CTRLXFER_PMCREGS_SIZE; pmc++)
    {    
	if (pmcstate->regs.reg[pmc] < vctx->last_pmc.regs.reg[pmc])
	{
	    old_pmc = 0;
	    new_pmc = pmcstate->regs.reg[pmc] + (~0UL - vctx->last_pmc.regs.reg[pmc]);
	}
	else
	{
	    old_pmc = vctx->last_pmc.regs.reg[pmc];
	    new_pmc = pmcstate->regs.reg[pmc];
	}
	diff_pmc = (new_pmc - old_pmc);
	access_energy +=  pmc_weight[pmc]  * diff_pmc;
	
	dprintf(debug_virq, "\tpmc %d new %x %x old %x %x diff %d\n", 
		pmc,  
		(L4_Word_t) (new_pmc >> 32), (L4_Word_t) new_pmc, 
		(L4_Word_t) (old_pmc >> 32), (L4_Word_t) old_pmc,
		(L4_Word_t) diff_pmc);
    }
    
    access_energy /= 1000 * PMC_DIVISOR;
    idle_energy   /= 1000 * PMC_DIVISOR;
    
    dprintf(debug_virq+1, "VIRQ account energy logid %d access %x idle %x\n", 
	    logid, (L4_Word_t) access_energy, (L4_Word_t) idle_energy);

    if (max_logid_in_use > EARM_MIN_LOGID)
	idle_energy /= (max_logid_in_use - EARM_MIN_LOGID + 1);
   
    for (L4_Word_t d = EARM_MIN_LOGID; d <= max_logid_in_use; d++) {
	resources[cpu].shared->clients[d].base_cost[cpu] += idle_energy;
    }
    
    manager_cpu_shared[cpu]->clients[logid].access_cost[cpu] += access_energy;
   
    for (L4_Word_t d = EARM_MIN_LOGID; d <= max_logid_in_use; d++) {
        manager_cpu_shared[cpu]->clients[d].base_cost[cpu] += idle_energy;
    }

    
}



/*****************************************************************
 * Module Counting
 *****************************************************************/

/* Interface Iounting::Resource */

IDL4_INLINE void IEarm_Resource_get_counter_implementation(CORBA_Object _caller, L4_Word_t *hi, L4_Word_t *lo,
							      idl4_server_environment *_env)
{
    /* implementation of Iounting::Resource::get_counter */
    L4_Word64_t counter = 0;
    L4_Word_t logid = tid_space_t::tid_to_space_id(_caller) + VM_LOGID_OFFSET;

    printf("CPU get_counter %d %x/%x\n", logid,  *hi, *lo);
    L4_KDB_Enter("Resource_get_counter called");
#if 0   
    for (L4_Word_t cpu = 0; cpu <= max_uuid_cpu; cpu++) {
	check_energy_abs(cpu, logid);

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
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
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
	    check_energy_abs(cpu, logid);
	}
    }
}



void earmcpu_init()
{
    if (!l4_pmsched_enabled)
    {
	/* Start resource thread */
	hthread_t *earmcpu_thread = get_hthread_manager()->create_thread( 
	    hthread_idx_earmcpu, 252, false,
	    IEarm_Resource_server);
	
	if( !earmcpu_thread )
	{
	    printf("\t earm couldn't start cpu accounting manager");
	    L4_KDB_Enter();
	return;
	}
	printf("\tearm cpu accounting manager TID %t\n", earmcpu_thread->get_global_tid());
	
	earmcpu_thread->start();
    }

}
 
