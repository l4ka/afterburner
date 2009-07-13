/*********************************************************************
 *                
 * Copyright (C) 2005-2006, 2009,  Karlsruhe University
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

#include <l4/ia32/arch.h>

#define VM_DOMAIN_OFFSET                                (L4_LOG_ROOTSERVER_DOMAIN + 1)

#define MHZ                                             (3000)

#define L4_LOG_TSC_PRECISION_DIVISOR		(1 << L4_LOG_TSC_PRECISION)

#define L4_LOG_TSC_TO_USEC(cycles)			((cycles * TSC_PRECISION_DIVISOR) / MHZ)
#define L4_LOG_TSC_TO_MSEC(cycles)			((cycles / (MHZ)) * TSC_PRECISION_DIVISOR / 1024)
#define L4_LOG_TSC_TO_SEC(cycles)			(((cycles / (MHZ * 1024)) * TSC_PRECISION_DIVISOR) / 1024)

/****************************************************************************************************************
 *					Logging Management
 ***************************************************************************************************************/

#define L4_LOG_FILE(cpu, domain, resource)              &l4_logfile_base[cpu][*(l4_logselector_base[cpu][domain][resource]) / sizeof(L4_Log_Ctrl_t)]
        
extern L4_Log_Ctrl_t    *l4_logfile_base[IResourcemon_max_cpus];
extern L4_Log_Sel_t     *l4_logselector_base[IResourcemon_max_cpus];

extern void logging_init();

#endif /* !__LOG_H__ */
