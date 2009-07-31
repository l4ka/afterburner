 /*********************************************************************
 *                
 * Copyright (C) 2005-2007, 2009,  Karlsruhe University
 *                
 * File path:     earm.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
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
#include <virq.h>

#include <ia32/msr.h>
#if !defined(__L4_IA32_ARCH_VM)
#include <ia32/l4archvm.h>
#endif

#include "earm_idl_server.h"

typedef IEarm_energy_t energy_t;

#define EARM_DEBUG
#undef  EARM_DEBUG_EVAL
#define EARM_DEBUG_MSEC                    1000
#define EARM_DEBUG_CPU
#define EARM_DEBUG_DISK
#define EARM_DEBUG_MIN_LOGID               0
#define EARM_DEBUG_MIN_RESOURCE		   0


#define EARM_MIN_LOGID                     0
#define EARMCPU_MSEC			  20
#define EARM_EASCPU_MSEC			 100
#define EARM_EASDISK_MSEC			 200

#define THROTTLE_DISK    // Disk throttling
#define THROTTLE_CPU    // CPU throttling

#define MIN_CPU_LOGID             3
#define DELTA_CPU_POWER         1000
#define INIT_CPU_POWER        100000

#define MIN_DISK_LOGID           4
#define DELTA_DISK_POWER         50
#define INIT_DISK_POWER        100000
#define INIT_DISK_THROTTLE     (6000)

#define DTF	4 // Disk throttle factor

#define EARM_UNLIMITED (~0ULL)

static const bool debug_earmdisk = 0;
extern L4_Word64_t debug_pmc[8];

extern void earm_init();
extern void earmmanager_init();
extern void earmcpu_init();
extern void earm_easmanager_init();

extern hthread_t *earmmanager_thread;

extern void earmmanager_debug(void *param ATTR_UNUSED_PARAM, hthread_t *htread ATTR_UNUSED_PARAM);
extern void earmcpu_collect();
extern void earmcpu_register( L4_ThreadId_t tid, L4_Word_t uuid_cpu, IEarm_shared_t **shared);

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

extern void earm_cpu_pmc_snapshot(L4_IA32_PMCCtrlXferItem_t *pmcstate);
extern void earm_cpu_update_records(word_t cpu, vm_context_t *vctx, L4_IA32_PMCCtrlXferItem_t *pmcstate);

#endif /* !__EARM_H__ */
