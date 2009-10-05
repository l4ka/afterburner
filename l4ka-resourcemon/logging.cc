/*********************************************************************
 *                
 * Copyright (C) 2005-2006, 2009,  Karlsruhe University
 *                
 * File path:     logging.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include "resourcemon.h"
#include "hthread.h"
#include "logging.h"
#include "vm.h"
#include <l4/kip.h>
#include <l4/schedule.h>
#include <l4/sigma0.h>

L4_LogCtrl_t      *l4_logfile_base[IResourcemon_max_cpus];
L4_LogSel_t       *l4_logselector_base[IResourcemon_max_cpus];

void parse_pmipc_logfile(word_t cpu, word_t logid)
{
    //L4_Word64_t most_recent_timestamp = 0;
    L4_Word_t count = 0;
    
    /*
     * Get logfile
     */
    L4_LogCtrl_t *c = L4_LogFile(l4_logfile_base[cpu], l4_logselector_base[cpu], logid, L4_LOG_RESOURCE_PMIPC);
    volatile L4_Word_t *current_idx = (L4_Word_t* ) (c + (c->X.current_offset / sizeof(L4_LogCtrl_t)));
    
    
    printf( "VIRQ pmipc ctrl reg %x current_idx @ %x = %x\n", c, current_idx, *current_idx);
    
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
	 * Logfile of preempted threads domain contains 
	 *	0       tid
	 * 	1	preempter
	 *	2	time hi
	 * 	3	time low 
	 *	4-14	GPREGS item
	 *	15	dummy
	 * read all state
	 */
	L4_ThreadId_t tid, preempter;
	tid.raw =  *current_idx;
	L4_LOG_DEC_LOG(current_idx, c);
	preempter.raw = *current_idx;
	L4_LOG_DEC_LOG(current_idx, c);

	printf("log says %t preempted by %t\n\t state: <" );

	for (L4_Word_t ctr=0; ctr < 2; ctr++)
	{
	    word_t s = *current_idx;
	    L4_LOG_DEC_LOG(current_idx, c);
	    printf("%x:", s);
	    s = *current_idx;
	    L4_LOG_DEC_LOG(current_idx, c);
	    printf("%x:", s);
	    s = *current_idx;
	    L4_LOG_DEC_LOG(current_idx, c);
	    printf("%x:", s);
	    s = *current_idx;
	    L4_LOG_DEC_LOG(current_idx, c);
	    printf("%x:", s);
	    s = *current_idx;
	    L4_LOG_DEC_LOG(current_idx, c);
	    printf("%x>\n\t<", s);
	}

    }
}


void logging_init() 
{

   
    L4_Fpage_t logfile_fp;
    L4_Word_t logselector_page;
    
    if (!l4_logging_enabled)
        return;

    printf("Init Logging \n");

    l4_cpu_cnt = L4_NumProcessors(l4_kip);
    l4_cpu_cnt = (l4_cpu_cnt > IResourcemon_max_cpus) ? IResourcemon_max_cpus : l4_cpu_cnt;
    
    l4_logfile_base[0] = (L4_LogCtrl_t *) L4_LogRegion(l4_kip);
    printf("\tlogfile_base[0] @ %p\n", l4_logfile_base[0]);
    
    logselector_page = L4_LogSelectorPage(l4_kip);
    
    for (L4_Word_t i=0; i <= logselector_page; i++)
    {
        logfile_fp = L4_Fpage((L4_Word_t) l4_logfile_base[0] + (PAGE_SIZE * i), PAGE_SIZE);
        L4_Sigma0_GetPage( L4_Pager(), logfile_fp, logfile_fp );
    }

    l4_logselector_base[0] = (L4_LogSel_t *) ( (L4_Word_t) l4_logfile_base[0] + (logselector_page * PAGE_SIZE)) ;
    printf("\tlogselector page[0] @ %p (%d)\n", l4_logselector_base[0], logselector_page);

   
    for (L4_Word_t cpu = 1; cpu < l4_cpu_cnt ; cpu++)
    {
	l4_logfile_base[cpu] = (L4_LogCtrl_t *) ((L4_Word_t) l4_logselector_base[cpu-1] + PAGE_SIZE);
        printf("\tlogfile_base[%d] @ %p\n", cpu, l4_logfile_base[cpu]);
        
        for (L4_Word_t i=0; i <= logselector_page; i++)
        {
            logfile_fp = L4_Fpage((L4_Word_t) l4_logfile_base[cpu] + (PAGE_SIZE * i), PAGE_SIZE);
            L4_Sigma0_GetPage( L4_Pager(), logfile_fp, logfile_fp );
        }

        l4_logselector_base[cpu] = (L4_LogSel_t *) ( (L4_Word_t) l4_logfile_base[cpu] + (logselector_page * PAGE_SIZE)) ;
        printf("\tlogselector page[%d] @ %p (%d)\n", cpu, l4_logselector_base[cpu], logselector_page);

    }

}

IDL4_INLINE void IResourcemon_request_logfiles_implementation(CORBA_Object _caller, L4_Word_t cpu, 
							     idl4_fpage_t *fp, idl4_server_environment *_env)
{
   
    if (cpu >= l4_cpu_cnt)
    {
        idl4_fpage_set_page( fp, L4_Nilpage );
	idl4_fpage_set_base( fp, 0 );
	idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
	idl4_fpage_set_permissions( fp,
				    IDL4_PERM_WRITE | IDL4_PERM_READ | IDL4_PERM_EXECUTE );
	return;
    }
        
    L4_Word_t logid = get_vm_allocator()->tid_to_vm(_caller)->get_space_id() + VM_LOGID_OFFSET;
    
    L4_LogCtrl_t *c = L4_LogFile(l4_logfile_base[cpu], l4_logselector_base[cpu], logid, L4_LOG_RESOURCE_PMC);
    L4_Word_t addr = (L4_Word_t) (c + (c->X.current_offset / sizeof(L4_LogCtrl_t))) & ~(L4_LOG_SIZE(c)-1);
    
    // Ensure that we have the page.
    *(volatile char *)addr;

    printf("%t logid %d requested log files cpu %d send %p\n", _caller, logid, cpu, addr);

						
    idl4_fpage_set_base( fp, addr );
    idl4_fpage_set_page( fp, L4_FpageLog2( addr, 13) );
    idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
    idl4_fpage_set_permissions( fp, IDL4_PERM_READ | IDL4_PERM_EXECUTE );
    
}
IDL4_PUBLISH_IRESOURCEMON_REQUEST_LOGFILES(IResourcemon_request_logfiles_implementation);


IDL4_INLINE void IResourcemon_request_log_control_regs_implementation(CORBA_Object _caller, const L4_Word_t cpu, 
                                                                      idl4_fpage_t *fp, idl4_server_environment *_env)

{

    L4_Word_t logid = get_vm_allocator()->tid_to_vm(_caller)->get_space_id() + VM_LOGID_OFFSET;
    L4_LogCtrl_t *c = L4_LogFile(l4_logfile_base[cpu], l4_logselector_base[cpu], logid, L4_LOG_RESOURCE_PMC);
    L4_Word_t addr = (L4_Word_t) c & PAGE_MASK;

   
    if (cpu >= l4_cpu_cnt)
    {
        idl4_fpage_set_page( fp, L4_Nilpage );
	idl4_fpage_set_base( fp, 0 );
	idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
	idl4_fpage_set_permissions( fp,
				    IDL4_PERM_WRITE | IDL4_PERM_READ | IDL4_PERM_EXECUTE );
	return;
    }

    // Ensure that we have the page.
    *(volatile char *)addr;
    
    printf("%t logid %d requested log control regs cpu %d send %p\n", _caller, logid, cpu, addr);
    
    //L4_KDB_Enter("Hypervisor log control reg request");
						
    idl4_fpage_set_base( fp, addr );
    idl4_fpage_set_page( fp, L4_FpageLog2( addr, 12) );
    idl4_fpage_set_mode( fp, IDL4_MODE_MAP );
    idl4_fpage_set_permissions( fp, IDL4_PERM_READ | IDL4_PERM_EXECUTE );
}
IDL4_PUBLISH_IRESOURCEMON_REQUEST_LOG_CONTROL_REGS(IResourcemon_request_log_control_regs_implementation);
