/*********************************************************************
 *                
 * Copyright (C) 2006-2007,  Karlsruhe University
 *                
 * File path:     earm_eas.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#include <l4/schedule.h>
#include "resourcemon.h"
#include "hconsole.h"
#include "hthread.h"
#include "locator.h"
#include "vm.h"
#include "string.h"
#include "earm.h"
#include "earm_idl_client.h"

hthread_t *eas_manager_thread;

static void earm_easmanager_throttle(
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    while (1) {
	asm("hlt\n");
	;
    }	
}


static earm_set_t diff_set, old_set;
static L4_Word_t disk_budget[L4_LOGGING_MAX_DOMAINS];
static L4_SignedWord64_t odt[L4_LOGGING_MAX_DOMAINS];
static L4_SignedWord64_t dt[L4_LOGGING_MAX_DOMAINS];

static L4_Word_t cpu_stride[UUID_IEarm_AccResCPU_Max][L4_LOGGING_MAX_DOMAINS];
static L4_Word_t cpu_budget[UUID_IEarm_AccResCPU_Max][L4_LOGGING_MAX_DOMAINS];


typedef struct earm_avg
{
    energy_t set[L4_LOGGING_MAX_DOMAINS];
} earm_avg_t;


static inline void update_energy(L4_Word_t res, L4_Word64_t time, earm_avg_t *avg)
{
    energy_t energy = 0;
    /* Get energy accounting data */
    for (L4_Word_t d = EARM_ACC_MIN_DOMAIN; d <= max_domain_in_use; d++) 
    {
	avg->set[d] = 0;
	    
	    
	for (L4_Word_t v = 0; v < UUID_IEarm_AccResMax; v++)
	{
	    /* Idle energy */
	    energy = resources[res].shared->clients[d].base_cost[v];
	    diff_set.res[res].clients[d].base_cost[v] = energy - old_set.res[res].clients[d].base_cost[v];
	    old_set.res[res].clients[d].base_cost[v] = energy;
	    diff_set.res[res].clients[d].base_cost[v] /= time;
	    //avg->set[d] += diff_set.res[res].clients[d].base_cost[v];

	    /* Access costs */
	    energy = resources[res].shared->clients[d].access_cost[v];
	    diff_set.res[res].clients[d].access_cost[v] = energy - old_set.res[res].clients[d].access_cost[v];
	    old_set.res[res].clients[d].access_cost[v] = energy;	
	    diff_set.res[res].clients[d].access_cost[v] /= time;
	    avg->set[d] += diff_set.res[res].clients[d].access_cost[v];
	    
	}
	
    }
}

static inline void print_energy(L4_Word_t res, earm_avg_t *avg)
{
    for (L4_Word_t d = EARM_ACC_MIN_DOMAIN; d <= max_domain_in_use; d++) 
    {
	if (avg->set[d])
	    hout << "d " << d << " u " << res
		 << " -> " << (L4_Word_t) avg->set[d] << "\n";
    }
}

earm_avg_t cpu_avg[UUID_IEarm_AccResCPU_Max], disk_avg;

static void earm_easmanager(
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    L4_Time_t sleep = L4_TimePeriod( EARM_ACCCPU_MSEC * 1000 );

    ASSERT(EARM_EASCPU_MSEC > EARM_ACCCPU_MSEC);
    const L4_Word_t earm_acccpu_per_eas_cpu = 
	EARM_EASCPU_MSEC / EARM_ACCCPU_MSEC;
    ASSERT(EARM_EASDISK_MSEC > EARM_ACCCPU_MSEC);
    const L4_Word_t earm_acccpu_per_eas_disk = 
	EARM_EASDISK_MSEC / EARM_ACCCPU_MSEC;

    
    L4_Word_t earm_acccpu_runs = 0;

    hout << "EARM: EAS manager " 
	 << " CPU " << earm_acccpu_per_eas_cpu << "\n"
	 << " DISK " << earm_acccpu_per_eas_disk << "\n";
    
    while (1) {
	earm_acccpu_collect();
	earm_acccpu_runs++;
	
#if defined(THROTTLE_DISK)    
	if (earm_acccpu_runs % earm_acccpu_per_eas_disk == 0)
	{
	    L4_Clock_t now_time = L4_SystemClock();
	    static L4_Clock_t last_time = { raw : 0};
	    L4_Word64_t usec = (now_time.raw - last_time.raw) ?:1;
	    last_time = now_time;
	    
	    update_energy(UUID_IEarm_AccResDisk, usec, &disk_avg); 
	    //print_energy(UUID_IEarm_AccResDisk, &disk_avg);
	    
	    for (L4_Word_t d = MIN_DISK_DOMAIN; d <= max_domain_in_use; d++)
	    {
		
		L4_Word_t cdt = dt[d];
		
		if (debug_earmdisk)
		    hout << "d " << d << " e " << disk_avg.set[d];
		
		if (disk_avg.set[d] > disk_budget[d])
		{
		    if (debug_earmdisk)
			hout << " > " << dt[d] << " o " << odt[d];
		    
		    if (dt[d] >= odt[d])
			dt[d] -= ((dt[d] - odt[d]) / DTF) + 1;
		    else 
			dt[d] -= (DTF * (odt[d] - dt[d]) / (DTF-1)) + 1;
		    
		    if (dt[d] <= 1)
			dt[d] = 1;


		}
		else if (disk_avg.set[d] < (disk_budget[d] - DELTA_DISK_POWER))
		{    
		    if (debug_earmdisk)
			hout << " < " << dt[d] << " o " << odt[d];
		    
		    if (dt[d] <= odt[d])
			dt[d] += ((odt[d] - dt[d]) / DTF) + 1;
		    else
			dt[d] += (DTF * (dt[d] - odt[d]) / (DTF-1)) + 1;
		    
		    if (dt[d] >= INIT_DISK_THROTTLE)
			dt[d] = INIT_DISK_THROTTLE;

		}		    
		odt[d] = cdt;

		if (debug_earmdisk)
		    hout << "\n";
		
		resources[UUID_IEarm_AccResDisk].shared->
		  clients[d].limit = dt[d];

		//resources[UUID_IEarm_AccResDisk].shared->
		//clients[d].limit = disk_budget[d];

	    }
	}
#endif

#if defined(THROTTLE_CPU)
	if (earm_acccpu_runs % earm_acccpu_per_eas_cpu == 0)
	{
	    L4_Clock_t now_time = L4_SystemClock();
	    static L4_Clock_t last_time = { raw : 0};
	    L4_Word64_t msec = ((now_time.raw - last_time.raw) / 1000) ?:1;
	    last_time = now_time;
	    
	    for (L4_Word_t c = 0; c < l4_cpu_cnt; c++)
	    {
		update_energy(c, msec, &cpu_avg[c]); 
		//print_energy(c, &cpu_avg[c]);
	    
		for (L4_Word_t d = MIN_CPU_DOMAIN; d <= max_domain_in_use; d++)
		{
		    L4_Word_t stride = cpu_stride[c][d];
		
		    if (cpu_avg[c].set[d] > cpu_budget[c][d])
			cpu_stride[c][d] +=20;
		    else if (cpu_avg[c].set[d] < (cpu_budget[c][d] - DELTA_CPU_POWER) && 
			     cpu_stride[c][d] > INIT_CPU_POWER)
			cpu_stride[c][d]-=20;	    
	
		    if (stride != cpu_stride[c][d])
		    {
			L4_Word_t result, sched_control, old_sched_control;
			vm_t * vm = get_vm_allocator()->space_id_to_vm( d - VM_DOMAIN_OFFSET );
	    
			//Restride domain
			stride = cpu_stride[c][d];
			sched_control = 16;
			old_sched_control = 0;
			//hout << "EARM: EAS restrides domain " << d << " TID " <<  vm->get_first_tid() 
			// << " to " << stride << "\n";
	    
			result = L4_ScheduleControl(vm->get_first_tid(), 
						    vm->get_first_tid(), 
						    stride, 
						    sched_control, 
						    &old_sched_control);
		    }
		}
	    }
	}
#endif
	L4_Sleep(sleep);
	
    }
}

static void earm_easmanager_control(
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    hout << "EARM: EAS controller TID " << L4_Myself() << "\n"; 
    L4_ThreadId_t tid;
    L4_MsgTag_t tag;
    L4_Word_t domain = 0;
    L4_Word_t resource = 0;
    L4_Word_t value = 0;
    
    while (1)
    {
	tag = L4_Wait(&tid);
	L4_StoreMR (1, &resource);
	L4_StoreMR (2, &domain);
	L4_StoreMR (3, &value);

	if (domain >= L4_LOGGING_MAX_DOMAINS ||
	    resource >= UUID_IEarm_AccResMax)
	{
	    hout << "EARM: invalid request"
		 << " resource " << resource  
		 << " domain " << domain
		 << " value " << value;
	    continue;
	}

	switch (resource)
	{
	case (UUID_IEarm_AccResDisk):
	    hout << "EARM: set disk budget" 
		 << " domain " << domain
		 << " to " << value
		 << "\n";
	    disk_budget[domain] = value;
	    break;
	case UUID_IEarm_AccResCPU_Min ... UUID_IEarm_AccResCPU_Max:
	    hout << "EARM: set CPU budget" 
		 << " domain " << domain
		 << " to " << value
		 << "\n";
	    cpu_budget[resource][domain] = value;
	    break;
	default:
	    hout << "EARM: unused resource " << resource  
		 << " domain " << domain
		 << " value " << value
		 << "\n";
	    break;
	}
	    
    }	
	
	
}
    
void earm_easmanager_init()
{
    
    /* Start resource manager thread */
    hthread_t *earm_easmanager_thread = get_hthread_manager()->create_thread( 
	hthread_idx_earm_easmanager, 252,
	earm_easmanager);

    if( !earm_easmanager_thread )
    {
	hout << "EARM: couldn't start EAS manager" ;
	L4_KDB_Enter();
	return;
    }
    hout << "EARM: EAS manager TID: " << earm_easmanager_thread->get_global_tid() << '\n';

    earm_easmanager_thread->start();

    /* Start debugger */
    hthread_t *earm_easmanager_control_thread = get_hthread_manager()->create_thread( 
	hthread_idx_earm_easmanager_control, 252,
	earm_easmanager_control);

    if( !earm_easmanager_control_thread )
    {
	hout << "EARM: couldn't start EAS management controller" ;
	L4_KDB_Enter();
	return;
    }
    hout << "EARM: EAS management controller TID: " << earm_easmanager_control_thread->get_global_tid() << '\n';
    earm_easmanager_control_thread->start();
    

    for (L4_Word_t d = 0; d < L4_LOGGING_MAX_DOMAINS; d++)
    {
	
	disk_avg.set[d] = 0;
	disk_budget[d] = INIT_DISK_POWER;
	dt[d] = INIT_DISK_THROTTLE;
    
	for (L4_Word_t u = 0; u < UUID_IEarm_AccResCPU_Max; u++)
	{
	    cpu_avg[u].set[d] = 0;
	    cpu_budget[u][d] = INIT_CPU_POWER;
	    cpu_stride[u][d] = INIT_CPU_STRIDE;
	}
    }

    L4_Word_t sched_control = 0, old_sched_control = 0, result = 0;
    L4_Word_t num_cpus = L4_NumProcessors(L4_GetKernelInterface());
    if (num_cpus > 1)
	L4_KDB_Enter("jsXXX: fix CPU throttling for smp");

    for (L4_Word_t cpu = 0 ; cpu < num_cpus ; cpu++)
    {
	/* Start throttler: */
	hthread_t *throttle_thread = get_hthread_manager()->create_thread( 
	    hthread_idx_e (hthread_idx_earm_easmanager_throttle+cpu), 
	    63, earm_easmanager_throttle);
	
	if( !throttle_thread )
	{
	    hout << "EARM: couldn't start EAS throttler" ;
	    L4_KDB_Enter();
	    return;
	}
	hout << "EARM: EAS throttler TID: " << throttle_thread->get_global_tid() << '\n';
	
	throttle_thread->start();
	//New Subqueue below me
	hout << "EARM: Create new subqueue below " <<  L4_Myself() 
	     << " for throttle thread " << throttle_thread->get_global_tid()
	     << "\n";
	
	sched_control = 1;
	result = L4_ScheduleControl(L4_Myself(), 
				    throttle_thread->get_global_tid(), 
				    63, 
				    sched_control, 
				    &old_sched_control);
	printf("result %d, old %x\n", result, old_sched_control);
	L4_Set_Priority(throttle_thread->get_global_tid(), 50);
	L4_Set_ProcessorNo(throttle_thread->get_global_tid(), cpu);
	//Restride
	L4_Word_t throttle_stride = 500;
	sched_control = 16;
	old_sched_control = 0;
	printf("EARM: Set stride for subqueue to %d \n", throttle_stride);
	result = L4_ScheduleControl(throttle_thread->get_global_tid(), 
				    throttle_thread->get_global_tid(),
				    throttle_stride, 
				    sched_control, &
				    old_sched_control);
	//printf("result %d, old %x\n", result, old_sched_control);
	//L4_KDB_Enter("Throttle thread creation");
    }
}

