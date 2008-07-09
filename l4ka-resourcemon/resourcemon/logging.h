/*********************************************************************
 *                
 * Copyright (C) 2005-2008,  Karlsruhe University
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
#include <l4/ia32/arch.h>

#define L4_LOGGING_MAX_CPUS		16
#undef LOGGING_MIGRATE_VMS
#define VM_DOMAIN_OFFSET		L4_LOGGING_ROOTSERVER_DOMAIN

#define VM_LD_MAX_RESOURCES		2
#define VM_MAX_RESOURCES		(1 <<  VM_LD_MAX_RESOURCES)

#define MHZ				(3000)

#define L4_LOGGING_TSC_PRECISION_DIVISOR		(1 << LOGGING_CPU_TSC_PRECISION)

#define L4_LOGGING_TSC_TO_USEC(cycles)			((cycles * TSC_PRECISION_DIVISOR) / MHZ)
#define L4_LOGGING_TSC_TO_MSEC(cycles)			((cycles / (MHZ)) * TSC_PRECISION_DIVISOR / 1024)
#define L4_LOGGING_TSC_TO_SEC(cycles)			(((cycles / (MHZ * 1024)) * TSC_PRECISION_DIVISOR) / 1024)

/****************************************************************************************************************
 *					Logging Management
 ***************************************************************************************************************/
extern logfile_control_t *l4_logfile_base[L4_LOGGING_MAX_CPUS];
extern u16_t *l4_selector_base[L4_LOGGING_MAX_CPUS];


#define L4_SELECTOR(cpu, domain, resource)				\
    (l4_selector_base[cpu] + (domain * L4_LOGGING_MAX_RESOURCES) + resource)



extern void logging_init();

#endif /* !__LOGGING_H__ */
