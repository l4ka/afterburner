/*********************************************************************
 *                
 * Copyright (C) 2006-2007, 2009-2010,  Karlsruhe University
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

#if defined(EARM_EAS_DEBUG_DISK)
static L4_Word_t eas_debug_disk_cnt = 0;
#define ON_EAS_DEBUG_DISK_INC() do { eas_debug_disk_cnt++; } while (0)
#define ON_EAS_DEBUG_DISK(x...) if (eas_debug_disk_cnt % 10 == 0) { x; } 
#else
#define ON_EAS_DEBUG_DISK_INC()
#define ON_EAS_DEBUG_DISK(x...) 
#endif
#if defined(EARM_EAS_DEBUG_CPU)
static L4_Word_t eas_debug_cpu_cnt = 0;
#define ON_EAS_DEBUG_CPU_INC() do { eas_debug_cpu_cnt++; } while (0)
#define ON_EAS_DEBUG_CPU(x...) if (eas_debug_cpu_cnt % 10 == 0) { x; } 
#else
#define ON_EAS_DEBUG_CPU_INC()
#define ON_EAS_DEBUG_CPU(x...) 
#endif


#define EARMCPU_PER_EAS_CPU   (EARM_EAS_CPU_MSEC / EARM_EAS_MSEC)
#define EARMCPU_PER_EAS_DISK  (EARM_EAS_DISK_MSEC / EARM_EAS_MSEC)


#if defined(THROTTLE)
static void earmeas_throttle(
    void *param UNUSED,
    hthread_t *htread UNUSED)
{
    while (1) {
	asm("hlt\n");
	
    }	
}
#endif

static earm_set_t diff_set, old_set;
static L4_SignedWord64_t odt[L4_LOG_MAX_LOGIDS];
static L4_SignedWord64_t dt[L4_LOG_MAX_LOGIDS];

L4_Word_t eas_disk_budget[L4_LOG_MAX_LOGIDS];
L4_Word_t eas_cpu_stride[UUID_IEarm_ResCPU_Max][L4_LOG_MAX_LOGIDS];
L4_Word_t eas_cpu_budget[UUID_IEarm_ResCPU_Max][L4_LOG_MAX_LOGIDS];


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
	
#if 1
        L4_Word_t v = res;
        energy = resources[res].shared->clients[d].access_cost[v];
        diff_set.res[res].clients[d].access_cost[v] = energy - old_set.res[res].clients[d].access_cost[v];
        old_set.res[res].clients[d].access_cost[v] = energy;	
        diff_set.res[res].clients[d].access_cost[v] /= time;
        avg->set[d] += diff_set.res[res].clients[d].access_cost[v];
#else
        for (L4_Word_t v = 0; v < UUID_IEarm_ResMax; v++)
#endif
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
    for (L4_Word_t d = EARM_MIN_LOGID; d <= max_logid_in_use; d++) 
    {
	if (avg->set[d])
	    printf("d %2d u %2d -> %6d\n", d, res, (L4_Word_t) avg->set[d]);
    }
}

earm_avg_t cpu_avg[UUID_IEarm_ResCPU_Max], disk_avg;

static void earmeas(
    void *param UNUSED,
    hthread_t *htread UNUSED)
{
    L4_Time_t sleep = L4_TimePeriod( EARM_EAS_MSEC * 1000 );


    
    L4_Word_t earmcpu_runs = 0;

    printf("EARM: EAS manager CPU %d DISK %d\n", EARMCPU_PER_EAS_CPU, EARMCPU_PER_EAS_DISK);
    
    while (1) {
	earmcpu_collect();
	earmcpu_runs++;
	
#if defined(THROTTLE_DISK)    
	if (earmcpu_runs % EARMCPU_PER_EAS_DISK == 0)
	{
	    L4_Clock_t now_time = L4_SystemClock();
	    static L4_Clock_t last_time = { raw : 0};
	    L4_Word64_t usec = (now_time.raw - last_time.raw) ? : 1;
	    last_time = now_time;
	    
	    update_energy(UUID_IEarm_ResDisk, usec, &disk_avg); 

            ON_EAS_DEBUG_DISK_INC();
	    ON_EAS_DEBUG_DISK(print_energy(UUID_IEarm_ResDisk, &disk_avg));
	    
	    for (L4_Word_t d = EARM_EAS_DISK_MIN_LOGID; d <= max_logid_in_use; d++)
	    {
		L4_Word_t cdt = dt[d];
                
		ON_EAS_DEBUG_DISK(printf("d %2d e %6d", d, disk_avg.set[d]));

		if (disk_avg.set[d] > eas_disk_budget[d])
		{
		    ON_EAS_DEBUG_DISK(printf(" > %6d d %6d o %6d", eas_disk_budget[d], dt[d], odt[d]));
                    
		    if (dt[d] >= odt[d])
			dt[d] -= ((dt[d] - odt[d]) / EARM_EAS_DISK_DTF) + 1;
		    else 
			dt[d] -= (EARM_EAS_DISK_DTF * (odt[d] - dt[d]) / (EARM_EAS_DISK_DTF-1)) + 1;
		    
		    if (dt[d] <= 1)
			dt[d] = 1;


		}
		else if (disk_avg.set[d] < (eas_disk_budget[d] - EARM_EAS_DISK_DELTA_PWR))
		{    
		    ON_EAS_DEBUG_DISK(printf(" < %6d d %6d o %6d", eas_disk_budget[d] - EARM_EAS_DISK_DELTA_PWR, dt[d], odt[d]));
		    if (dt[d] <= odt[d])
			dt[d] += ((odt[d] - dt[d]) / EARM_EAS_DISK_DTF) + 1;
		    else
			dt[d] += (EARM_EAS_DISK_DTF * (dt[d] - odt[d]) / (EARM_EAS_DISK_DTF-1)) + 1;
		    
		    if (dt[d] >= EARM_EAS_DISK_THROTTLE)
			dt[d] = EARM_EAS_DISK_THROTTLE;

		}		    
		odt[d] = cdt;

		ON_EAS_DEBUG_DISK(printf(" -> dt %d\n", (L4_Word_t) dt[d]));
		resources[UUID_IEarm_ResDisk].shared->clients[d].limit = dt[d];

		//resources[UUID_IEarm_ResDisk].shared->
		//clients[d].limit = eas_disk_budget[d];

	    }
	}
#endif

#if defined(THROTTLE_CPU)
	if (earmcpu_runs % EARMCPU_PER_EAS_CPU == 0)
	{
	    L4_Clock_t now_time = L4_SystemClock();
	    static L4_Clock_t last_time = { raw : 0};
	    L4_Word64_t msec = ((now_time.raw - last_time.raw) / 1000) ?:1;
	    last_time = now_time;
            
            ON_EAS_DEBUG_CPU_INC();

	    for (L4_Word_t c = 0; c < l4_cpu_cnt; c++)
	    {
		update_energy(c, msec, &cpu_avg[c]); 
		//print_energy(c, &cpu_avg[c]);
	    
		for (L4_Word_t d = EARM_EAS_CPU_MIN_LOGID; d <= max_logid_in_use; d++)
		{
		    L4_Word_t stride = eas_cpu_stride[c][d];
		
		    if (cpu_avg[c].set[d] > eas_cpu_budget[c][d])
			eas_cpu_stride[c][d] +=20;
		    else if (cpu_avg[c].set[d] < (eas_cpu_budget[c][d] - EARM_EAS_CPU_DELTA_PWR) && 
			     eas_cpu_stride[c][d] > EARM_EAS_CPU_INIT_PWR)
			eas_cpu_stride[c][d]-=20;	    
	
		    if (stride != eas_cpu_stride[c][d])
		    {
			L4_Word_t result, sched_control;
			vm_t * vm = get_vm_allocator()->space_id_to_vm( d - VM_LOGID_OFFSET );
	    
			//Restride logid
			stride = eas_cpu_stride[c][d];
			sched_control = 16;
                        
                        ON_EAS_DEBUG_CPU(printf("EARM: restrides logid %d TID %t stride %d\n", d, vm->get_first_tid(), stride));
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

   
void earmeas_init()
{

    for (L4_Word_t d = 0; d < L4_LOG_MAX_LOGIDS; d++)
    {
	
	disk_avg.set[d] = 0;
	eas_disk_budget[d] = EARM_EAS_CPU_INIT_PWR;
	dt[d] = EARM_EAS_DISK_THROTTLE;
    
	for (L4_Word_t u = 0; u < UUID_IEarm_ResCPU_Max; u++)
	{
	    cpu_avg[u].set[d] = 0;
	    eas_cpu_budget[u][d] = EARM_EAS_CPU_INIT_PWR;
	    eas_cpu_stride[u][d] = INIT_CPU_STRIDE;
	}

   
    }

   
    if (l4_pmsched_enabled || !l4_logging_enabled)
	return;

    /* Start resource manager thread */
    hthread_t *earmeas_thread = get_hthread_manager()->create_thread( 
	hthread_idx_earmeas, 252, false, earmeas);

    if( !earmeas_thread )
    {
	printf("\t earm couldn't start EAS manager");
	L4_KDB_Enter();
	return;
    }
    printf("\tEAS manager TID: %t\n", earmeas_thread->get_global_tid());

    earmeas_thread->start();


#if defined(THROTTLE_CPU)
    L4_Word_t sched_control = 0, result = 0;
    if (l4_cpu_cnt > 1)
	L4_KDB_Enter("jsXXX: fix CPU throttling for smp");
    
    for (L4_Word_t cpu = 0 ; cpu < l4_cpu_cnt ; cpu++)
    {
	/* Start throttler: */
	hthread_t *throttle_thread = get_hthread_manager()->create_thread( 
	    hthread_idx_e (hthread_idx_earmeas_throttle+cpu), 
	    90, false, earmeas_throttle);
	
	if( !throttle_thread )
	{
	    printf("EARM: couldn't start EAS throttler\n");
	    L4_KDB_Enter();
	    return;
	}
        printf("\tEAS throttler TID: %t\n", throttle_thread->get_global_tid());
	L4_Set_Logid(throttle_thread->get_global_tid(), 1);
	
	throttle_thread->start();
	//New Subqueue below me
        printf("\tEAS create new subqueue below %t for throttler TID: %t\n", 
               L4_Myself(), throttle_thread->get_global_tid());
	
	sched_control = 1;
        result = L4_HS_Schedule(throttle_thread->get_global_tid(), 
                                sched_control,
                                L4_Myself(), 
                                98, 500, 
                                &sched_control);
        
	L4_Set_ProcessorNo(throttle_thread->get_global_tid(), cpu);
    }
#endif
}

