/*********************************************************************
 *                
 * Copyright (C) 2005-2007,  Karlsruhe University
 *                
 * File path:     logging.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <common/hthread.h>
#include <l4/ia32/logging.h>

#undef LOGGING_MIGRATE_VMS
#define VM_DOMAIN_OFFSET		L4_LOGGING_ROOTTASK_DOMAIN

#define VM_LD_MAX_LOAD_PARMS	0
#define VM_MAX_LOAD_PARMS	(1 <<  VM_LD_MAX_LOAD_PARMS)
#define VM_LD_MAX_RESOURCES	2
#define VM_MAX_RESOURCES	(1 <<  VM_LD_MAX_RESOURCES)

#define VM_LOAD_RUNQUEUE	0
#define VM_RESOURCE_SHM		0

#define MHZ					(3000)

#define L4_LOGGING_TSC_PRECISION_DIVISOR		(1 << L4_LOGGING_TSC_PRECISION)

#define L4_LOGGING_TSC_TO_USEC(cycles)			((cycles * TSC_PRECISION_DIVISOR) / MHZ)
#define L4_LOGGING_TSC_TO_MSEC(cycles)			((cycles / (MHZ)) * TSC_PRECISION_DIVISOR / 1024)
#define L4_LOGGING_TSC_TO_SEC(cycles)			(((cycles / (MHZ * 1024)) * TSC_PRECISION_DIVISOR) / 1024)

/****************************************************************************************************************
 *					Logging Management
 ***************************************************************************************************************/
extern logfile_control_t *l4_logfile_base[L4_LOGGING_MAX_CPUS];
extern log_event_control_t *l4_loadselector_base[L4_LOGGING_MAX_CPUS];
extern log_event_control_t *l4_resourceselector_base[L4_LOGGING_MAX_CPUS];

extern void logging_init();

#endif /* !__LOGGING_H__ */
