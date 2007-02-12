/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     logging.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#include <l4/sigma0.h>
#include <common/hconsole.h>
#include <resourcemon/logging.h>

logfile_control_t *l4_logfile_base[L4_LOGGING_MAX_CPUS];
log_event_control_t *l4_loadselector_base[L4_LOGGING_MAX_CPUS];
log_event_control_t *l4_resourceselector_base[L4_LOGGING_MAX_CPUS];

void logging_init() 
{

    hout << "Init Logging \n";

    log_event_control_t **selector_ptr;
    L4_Fpage_t logfile_fp;
    
    void *kip = L4_GetKernelInterface();
    L4_Word_t l4_cpu_cnt = L4_NumProcessors(L4_GetKernelInterface());
    l4_cpu_cnt = (l4_cpu_cnt > IResourcemon_max_cpus) ? IResourcemon_max_cpus : l4_cpu_cnt;
    
    l4_logfile_base[0] = (logfile_control_t *) L4_LogRegion(kip);
    hout << "logfile_base[0] @ " << (void *)l4_logfile_base[0] << '\n';
    
    logfile_fp = L4_Fpage((L4_Word_t) l4_logfile_base[0], PAGE_SIZE * 64);
    L4_Sigma0_GetPage( L4_Pager(), logfile_fp, logfile_fp );


    selector_ptr = (log_event_control_t **) 
	( (L4_Word_t) l4_logfile_base[0] + (L4_LogSelectorPage(kip) * PAGE_SIZE)) ;
    hout << "selector page[0] @ " << (void *)selector_ptr << '\n';

    l4_loadselector_base[0] = selector_ptr[0];
    l4_resourceselector_base[0] = selector_ptr[1];
    
    hout << "load selectors[0] @ " << (void *)l4_loadselector_base[0] << '\n';
    hout << "resource selectors[0] @ " << (void *)l4_resourceselector_base[0] << '\n';

    
    for (L4_Word_t cpu = 1; cpu < l4_cpu_cnt ; cpu++)
    {
	l4_logfile_base[cpu] = (logfile_control_t *) ((L4_Word_t) selector_ptr + PAGE_SIZE);
	hout << "logfile_base[" << cpu << "] @ " << (void *)l4_logfile_base[cpu] << '\n';

	logfile_fp = L4_Fpage((L4_Word_t) l4_logfile_base[cpu], PAGE_SIZE * 64);
	L4_Sigma0_GetPage( L4_Pager(), logfile_fp, logfile_fp );

	selector_ptr = (log_event_control_t **) 
	    ( (L4_Word_t) l4_logfile_base[cpu] + (L4_LogSelectorPage(kip) * PAGE_SIZE)) ;
	hout << "selector page[" << cpu << "] @ " << (void *)selector_ptr << '\n';

	l4_loadselector_base[cpu] = selector_ptr[0];
	l4_resourceselector_base[cpu] = selector_ptr[1];
	
	hout << "load selectors[" << cpu << "] @ " << (void *)l4_loadselector_base[cpu] << '\n';
	hout << "resource selectors[" << cpu << "] @ " << (void *)l4_resourceselector_base[cpu] << '\n';
    }

}
