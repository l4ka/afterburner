 /*********************************************************************
 *                
 * Copyright (C) 2005-2007, 2009,  Karlsruhe University
 *                
 * File path:     earm.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id: earm.h,v 1.1 2009/10/07 17:43:47 stoess Exp stoess $
 *                
 ********************************************************************/
#ifndef __EARM_H__
#define __EARM_H__

#include <l4/types.h>
#include <l4/schedule.h>
#include <l4/ia32/arch.h>

#include <vm.h>
#include <hthread.h>
#include <resourcemon.h>
#include <string.h>
#include <logging.h>

#include <ia32/msr.h>

#include "earm_idl_server.h"

typedef IEarm_energy_t energy_t;
#define EARM_UNLIMITED (~0ULL)

#define EARM_MIN_LOGID                   0

#define EARM_CPU_DIVISOR                 100
#define EARM_CPU_EXP		          20
#define EARM_VIRQ_TICKS                   20

#define EARM_MGR_PRINT
#define EARM_MGR_PRINT_MSEC            1000

#define EARM_EAS_MSEC			  20
#define EARM_EAS_CPU_MSEC		 100
#define EARM_EAS_DISK_MSEC		 200
#undef  EARM_EAS_DEBUG_CPU
#undef  EARM_EAS_DEBUG_DISK

#define THROTTLE_DISK    // Disk throttling
#define THROTTLE_CPU    // CPU throttling

#define EARM_EAS_CPU_MIN_LOGID            3
#define EARM_EAS_CPU_DELTA_PWR         1000
#define EARM_EAS_CPU_INIT_PWR        100000

#define EARM_EAS_DISK_MIN_LOGID           4
#define EARM_EAS_DISK_DELTA_PWR         1000
#define EARM_EAS_DISK_INIT_PWR        100000
#define EARM_EAS_DISK_THROTTLE   (6000)

#define EARM_EAS_DISK_DTF	4 // Disk throttle factor


extern void earm_init();
extern void earmmanager_init();
extern void earmcpu_init();
extern void earmeas_init();

extern hthread_t *earmmanager_thread;


extern void earmmanager_debug(void *param UNUSED, hthread_t *htread UNUSED);
extern void earmcpu_collect();
extern void earmcpu_register( L4_ThreadId_t tid, L4_Word_t uuid_cpu, IEarm_shared_t **shared);
extern void earmcpu_pmc_snapshot(L4_IA32_PMCCtrlXferItem_t *pmcstate);
extern void earmcpu_update(L4_Word_t cpu, L4_Word_t logid, 
			   L4_IA32_PMCCtrlXferItem_t *pmcstate,
			   L4_IA32_PMCCtrlXferItem_t *lpmcstate,
			   L4_Word64_t *tsc, L4_Word64_t *energy);

#if defined(EARM_MGR_PRINT)
extern void earmmanager_print_resources();
#endif

extern L4_Word_t eas_disk_budget[L4_LOG_MAX_LOGIDS];
extern L4_Word_t eas_cpu_stride[UUID_IEarm_ResCPU_Max][L4_LOG_MAX_LOGIDS];
extern L4_Word_t eas_cpu_budget[UUID_IEarm_ResCPU_Max][L4_LOG_MAX_LOGIDS];

extern L4_Word_t max_uuid_cpu;

/* Shared resource accounting data */
typedef struct {
    L4_ThreadId_t tid;
    IEarm_shared_t *shared;
} resource_t;
extern resource_t resources[UUID_IEarm_ResMax];

typedef struct earm_set
{
    IEarm_shared_t res[UUID_IEarm_ResMax];
} earm_set_t;

// Some additional MSRs not found in msr.h
#define MSR_IA32_PEBS_ENABLE                    0x3f1
#define MSR_IA32_PEBS_MATRIX_VERT               0x3f2

static const L4_Word64_t pmc_weight[8] = { L4_X86_PMC_TSC_WEIGHT, 
                                           L4_X86_PMC_UC_WEIGHT,  
                                           L4_X86_PMC_MQW_WEIGHT, 
                                           L4_X86_PMC_RB_WEIGHT, 
                                           L4_X86_PMC_MB_WEIGHT,  
                                           L4_X86_PMC_MR_WEIGHT,  
                                           L4_X86_PMC_MLR_WEIGHT, 
                                           L4_X86_PMC_LDM_WEIGHT };


#endif /* !__EARM_H__ */
