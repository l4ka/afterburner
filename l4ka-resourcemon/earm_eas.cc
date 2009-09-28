/*********************************************************************
 *                
 * Copyright (C) 2006-2007, 2009,  Karlsruhe University
 *                
 * File path:     earm_eas.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include "earm.h"
#include "earm_idl_client.h"

hthread_t *eas_manager_thread;

static void earm_easmanager_throttle(
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    L4_KDB_Enter("Throttler" );
    while (1) {
	//asm("hlt\n");
	;
    }	
}


static earm_set_t diff_set, old_set;
static L4_Word_t disk_budget[L4_LOG_MAX_LOGIDS];
static L4_SignedWord64_t odt[L4_LOG_MAX_LOGIDS];
static L4_SignedWord64_t dt[L4_LOG_MAX_LOGIDS];

static L4_Word_t cpu_stride[UUID_IEarm_ResCPU_Max][L4_LOG_MAX_LOGIDS];
static L4_Word_t cpu_budget[UUID_IEarm_ResCPU_Max][L4_LOG_MAX_LOGIDS];


typedef struct earm_avg
{
    energy_t set[L4_LOG_MAX_LOGIDS];
} earm_avg_t;


static inline void update_energy(L4_Word_t res, L4_Word64_t time, earm_avg_t *avg)
{
    energy_t energy = 0;
    /* Get energy accounting data */
    for (L4_Word_t d = EARM_MIN_LOGID; d <= max_logid_in_use; d++) 
    {
	avg->set[d] = 0;
	    
	    
	for (L4_Word_t v = 0; v < UUID_IEarm_ResMax; v++)
	{
	    /* Idle energy */
	    energy = resources[res].shared->clients[d].base_cost[v];
	    diff_set.res[res].clients[d].base_cost[v] = energy - old_set.res[res].clients[d].base_cost[v];
	    old_set.res[res].clients[d].base_cost[v] = energy;
	    diff_set.res[res].clients[d].base_cost[v] /= time;
	    //avg->set[d] += diff_set.res[res].clients[d].base_cost[v];

	    /* ess costs */
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
    for (L4_Word_t d = EARM_MIN_LOGID; d <= max_logid_in_use; d++) 
    {
	if (avg->set[d])
	    printf("d %d u %d -> %d\n", d, res, (L4_Word_t) avg->set[d]);
    }
}

earm_avg_t cpu_avg[UUID_IEarm_ResCPU_Max], disk_avg;

static void earm_easmanager(
    void *param ATTR_UNUSED_PARAM,
    hthread_t *htread ATTR_UNUSED_PARAM)
{
    L4_Time_t sleep = L4_TimePeriod( EARMCPU_MSEC * 1000 );

    ASSERT(EARM_EASCPU_MSEC > EARMCPU_MSEC);
    const L4_Word_t earmcpu_per_eas_cpu = 
	EARM_EASCPU_MSEC / EARMCPU_MSEC;
    ASSERT(EARM_EASDISK_MSEC > EARMCPU_MSEC);
    const L4_Word_t earmcpu_per_eas_disk = 
	EARM_EASDISK_MSEC / EARMCPU_MSEC;

    
    L4_Word_t earmcpu_runs = 0;

    printf("EARM: EAS manager CPU %d DISK %d", earmcpu_per_eas_cpu, earmcpu_per_eas_disk);
    
    while (1) {
	earmcpu_collect();
	earmcpu_runs++;
	
#if defined(THROTTLE_DISK)    
	if (earmcpu_runs % earmcpu_per_eas_disk == 0)
	{
	    L4_Clock_t now_time = L4_SystemClock();
	    static L4_Clock_t last_time = { raw : 0};
	    L4_Word64_t usec = (now_time.raw - last_time.raw) ?:1;
	    last_time = now_time;
	    
	    update_energy(UUID_IEarm_ResDisk, usec, &disk_avg); 
	    //print_energy(UUID_IEarm_ResDisk, &disk_avg);
	    
	    for (L4_Word_t d = MIN_DISK_LOGID; d <= max_logid_in_use; d++)
	    {
		
		L4_Word_t cdt = dt[d];
		
		if (debug_earmdisk)
                    printf("d %d e %d", d, disk_avg.set[d]);
		
		if (disk_avg.set[d] > disk_budget[d])
		{
		    if (debug_earmdisk)
			printf(" > %d o %d", dt[d], odt[d]);
                    
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
			printf(" < %d o %d\n", dt[d], odt[d]); 
		    
		    if (dt[d] <= odt[d])
			dt[d] += ((odt[d] - dt[d]) / DTF) + 1;
		    else
			dt[d] += (DTF * (dt[d] - odt[d]) / (DTF-1)) + 1;
		    
		    if (dt[d] >= INIT_DISK_THROTTLE)
			dt[d] = INIT_DISK_THROTTLE;

		}		    
		odt[d] = cdt;

		if (debug_earmdisk)
		    printf("\n");
		
		resources[UUID_IEarm_ResDisk].shared->
		  clients[d].limit = dt[d];

		//resources[UUID_IEarm_ResDisk].shared->
		//clients[d].limit = disk_budget[d];

	    }
	}
#endif

#if defined(THROTTLE_CPU)
	if (earmcpu_runs % earmcpu_per_eas_cpu == 0)
	{
	    L4_Clock_t now_time = L4_SystemClock();
	    static L4_Clock_t last_time = { raw : 0};
	    L4_Word64_t msec = ((now_time.raw - last_time.raw) / 1000) ?:1;
	    last_time = now_time;
	    
	    for (L4_Word_t c = 0; c < l4_cpu_cnt; c++)
	    {
		update_energy(c, msec, &cpu_avg[c]); 
		//print_energy(c, &cpu_avg[c]);
	    
		for (L4_Word_t d = MIN_CPU_LOGID; d <= max_logid_in_use; d++)
		{
		    L4_Word_t stride = cpu_stride[c][d];
		
		    if (cpu_avg[c].set[d] > cpu_budget[c][d])
			cpu_stride[c][d] +=20;
		    else if (cpu_avg[c].set[d] < (cpu_budget[c][d] - DELTA_CPU_POWER) && 
			     cpu_stride[c][d] > INIT_CPU_POWER)
			cpu_stride[c][d]-=20;	    
	
		    if (stride != cpu_stride[c][d])
		    {
			L4_Word_t result, sched_control;
			vm_t * vm = get_vm_allocator()->space_id_to_vm( d - VM_LOGID_OFFSET );
	    
			//Restride logid
			stride = cpu_stride[c][d];
			sched_control = 16;
                        
                        printf("EARM: restrides logid %d TID %t stride %d\n", d, vm->get_first_tid(), stride);

                        result = L4_HS_Schedule(vm->get_first_tid(), sched_control, vm->get_first_tid(), 0, stride, &sched_control);
                        
                        if (!result)
                        {
                            printf("Error: failure setting scheduling stride for VM TID %t result %d, errcode %s",
                                   vm->get_first_tid(), result, L4_ErrorCode_String(L4_ErrorCode()));
                            L4_KDB_Enter("EARM scheduling error");
                        }	
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
    L4_ThreadId_t tid;
    L4_MsgTag_t tag;
    L4_Word_t logid = 0;
    L4_Word_t resource = 0;
    L4_Word_t value = 0;
    
    while (1)
    {
	tag = L4_Wait(&tid);
	L4_StoreMR (1, &resource);
	L4_StoreMR (2, &logid);
	L4_StoreMR (3, &value);

	if (logid >= L4_LOG_MAX_LOGIDS || (resource >= UUID_IEarm_ResMax && resource != 100))
	{
	    printf("EARM: invalid  request resource %d logid %d value %d\n", 
                   resource, logid, value);
	    continue;
	}

	switch (resource)
	{
	case (UUID_IEarm_ResDisk):
	    printf("EARM: set disk budget resource %d logid %d value %d\n", 
                   resource, logid, value);
	    disk_budget[logid] = value;
	    break;
	case UUID_IEarm_ResCPU_Min ... UUID_IEarm_ResCPU_Max:
	    printf("EARM: set CPU budget resource %d logid %d value %d\n", 
                   resource, logid, value);
	    cpu_budget[resource][logid] = value;
	    break;
	case 100:
	    printf("EARM: set CPU stride resource %d logid %d value %d\n", 
                   resource, logid, value);
            {
                L4_Word_t stride, result, sched_control;
                vm_t * vm = get_vm_allocator()->space_id_to_vm( logid - VM_LOGID_OFFSET );
	    
                //Restride logid
                stride = value;
                sched_control = 16;
                        
                result = L4_HS_Schedule(vm->get_first_tid(), sched_control, vm->get_first_tid(), 0, stride, &sched_control);
                        
                if (!result)
                {
                    printf("Error: failure setting scheduling stride for VM TID %t result %d, errcode %s",
                           vm->get_first_tid(), result, L4_ErrorCode_String(L4_ErrorCode()));
                    L4_KDB_Enter("EARM scheduling error");
                }	
            }
	    break;
	default:
	    printf("EARM: unused resource %d logid %d value %d\n", 
                   resource, logid, value);
	    break;
	}
	    
    }	
	
	
}
    
void earm_easmanager_init()
{

    for (L4_Word_t d = 0; d < L4_LOG_MAX_LOGIDS; d++)
    {
	
	disk_avg.set[d] = 0;
	disk_budget[d] = INIT_DISK_POWER;
	dt[d] = INIT_DISK_THROTTLE;
    
	for (L4_Word_t u = 0; u < UUID_IEarm_ResCPU_Max; u++)
	{
	    cpu_avg[u].set[d] = 0;
	    cpu_budget[u][d] = INIT_CPU_POWER;
	    cpu_stride[u][d] = INIT_CPU_STRIDE;
	}
    }


    if (!l4_pmsched_enabled)
    {

	/* Start resource manager thread */
	hthread_t *earm_easmanager_thread = get_hthread_manager()->create_thread( 
	    hthread_idx_earm_easmanager, 252, false, earm_easmanager);

	if( !earm_easmanager_thread )
	{
	    printf("\tEARM couldn't start EAS manager");
	    L4_KDB_Enter();
	    return;
	}
	printf("\tEAS manager TID: %t\n", earm_easmanager_thread->get_global_tid());

	earm_easmanager_thread->start();

	/* Start debugger */
	hthread_t *earm_easmanager_control_thread = get_hthread_manager()->create_thread( 
	    hthread_idx_earm_easmanager_control, 252, false, earm_easmanager_control);

	if( !earm_easmanager_control_thread )
	{
	    printf("\t earm couldn't start EAS management controller");
	    L4_KDB_Enter();
	    return;
	}
	printf("\tEAS management controller TID: %t\n", earm_easmanager_control_thread->get_global_tid());
	earm_easmanager_control_thread->start();
    
    }
    
    L4_Word_t sched_control = 0, result = 0;
    if (l4_cpu_cnt > 1)
	L4_KDB_Enter("jsXXX: fix CPU throttling for smp");

    if (l4_pmsched_enabled)
	return;
    
    for (L4_Word_t cpu = 0 ; cpu < l4_cpu_cnt ; cpu++)
    {
	/* Start throttler: */
	hthread_t *throttle_thread = get_hthread_manager()->create_thread( 
	    hthread_idx_e (hthread_idx_earm_easmanager_throttle+cpu), 
	    90, false, earm_easmanager_throttle);
	
	if( !throttle_thread )
	{
	    printf("EARM: couldn't start EAS throttler\n");
	    L4_KDB_Enter();
	    return;
	}
	
	printf("\tEAS throttler TID: %t\n", throttle_thread->get_global_tid());
	
	if (l4_hsched_enabled)
	{
		
	    throttle_thread->start();
	    //New Subqueue below me
	    printf("EARM: EAS create new subqueue below %t for throttler TID: %t\n", 
		   L4_Myself(), throttle_thread->get_global_tid());
		
	    sched_control = 1;
	    result = L4_HS_Schedule(throttle_thread->get_global_tid(), 
				    sched_control,
				    L4_Myself(), 
				    98, 500, 
				    &sched_control);
        
	    L4_Set_ProcessorNo(throttle_thread->get_global_tid(), cpu);
	}
	    
	throttle_thread->start();
    }
}

