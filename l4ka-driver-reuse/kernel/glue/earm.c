/*********************************************************************
 *                
 * Copyright (C) 2009-2010,  Karlsruhe University
 *                
 * File path:     glue/earm.c
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/

#include <l4/types.h>
#include <l4/kip.h>
#include <l4/kdebug.h>
#include <l4/ia32/arch.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <glue/vmserver.h>

//L4_ThreadId_t res_cpu_tid;
#define IA32_PAGE_SIZE		4096
#define VM_LOGID_OFFSET                                (L4_LOG_ROOTSERVER_LOGID + 1)

#define ACCOUNT_IDLE
#define EARM_CPU_DIVISOR                 100

#define MHZ	3000

#define DUMP_ENERGY()                                                   \
    {                                                                   \
        static L4_Word64_t dbg_sum_energy, dbg_energy;                  \
        static L4_Word64_t dbg_tsc = 0;                                 \
        L4_Word64_t tsc, ms = 0;                                        \
        rdtsc64(tsc);                                                   \
        ms = tsc - dbg_tsc;                                             \
        do_div(ms, MHZ * 1000);                                         \
                                                                        \
        dbg_energy = energy;                                            \
        do_div(dbg_energy, 1000);                                       \
        dbg_sum_energy += dbg_energy;                                   \
                                                                        \
                                                                        \
        if (ms >= 1000)                                                 \
        {                                                               \
            L4_Word64_t dbg_sum_energy_ms = dbg_sum_energy;             \
            do_div(dbg_sum_energy_ms, ms);                              \
                                                                        \
            printk("logid %d (cpu %d tdiff %d): energy %u\n",           \
                   (u32) l4_log_logid,                                  \
                   (u32) cpu,                                           \
                   (u32) ms,                                            \
                   (u32) dbg_sum_energy_ms);                            \
            dbg_tsc = tsc;                                              \
            dbg_sum_energy = 0;                                         \
        }                                                               \
    }

#define DUMP_LOGFILE()                                                  \
    printk("base %x regbase %x log control reg %x offset %x current_ofs %x current_idx %x sz %u (%u) cpu %u\n", \
           (u32) l4_pmc_logfiles[cpu], (u32) l4_log_control_regs[cpu],  \
           (u32) c, (u32) o, (u32) c->X.current_offset,                 \
           (u32) current_idx,                                           \
           (u32) L4_LOG_ENTRIES(c), (u32) L4_LOG_SIZE(c),               \
       (u32) cpu); 
	

#define DUMP_COUNTERS()                                                 \
    for (ctr=0; ctr < 8; ctr++)                                         \
    {                                                                   \
        printk("pmc %d\texit %8x %8x \t- entry %8x / %8x = delta %8x\n", (u32) ctr, \
               (u32) (exit_pmc[ctr] >> 32), (u32) exit_pmc[ctr],        \
               (u32) (entry_pmc[ctr] >> 32), (u32) entry_pmc[ctr],      \
               (u32) (exit_pmc[ctr] - entry_pmc[ctr]));                 \
    }


#include "resourcemon_idl_client.h"
#include "earm_idl_client.h"

L4_ThreadId_t L4VM_earm_manager_tid;

typedef struct energy_pmc
{
    // PMC states
    L4_Word64_t tsc;              // time stamp counter
    L4_Word64_t uc;               // unhalted cycles
    L4_Word64_t mqw;              // mmop queue writes
    L4_Word64_t rb;               // retired branches
    L4_Word64_t mb;               // mispredicted branches
    L4_Word64_t mr;               // memory retired
    L4_Word64_t mlr;              // mob load replay
    L4_Word64_t ldm;              // ld miss 1l retired
} energy_pmc_t;

typedef struct energy_pmc32
{
    // PMC states
    L4_Word_t tsc;              // time stamp counter
    L4_Word_t uc;               // unhalted cycles
    L4_Word_t mqw;              // mmop queue writes
    L4_Word_t rb;               // retired branches
    L4_Word_t mb;               // mispredicted branches
    L4_Word_t mr;               // memory retired
    L4_Word_t mlr;              // mob load replay
    L4_Word_t ldm;              // ld miss 1l retired
    
} energy_pmc32_t;


energy_pmc_t last_pmc;

#define rdtsc64(val) __asm__ __volatile__ ( "rdtsc" : "=A" (val) )
#define rdpmc64(pmc, val) __asm__ __volatile__ ("rdpmc" : "=A" (val) : "c" (pmc))


L4_LogCtrl_t l4_log_control_reg_page[IResourcemon_max_cpus]\
		  [L4_LOG_SELECTOR_SIZE / sizeof(L4_LogCtrl_t)] __attribute__((aligned(L4_LOG_SELECTOR_SIZE)));
L4_LogCtrl_t *l4_log_control_regs[IResourcemon_max_cpus]  __attribute__((aligned(L4_LOG_SELECTOR_SIZE)));

#define L4_PMC_LOGFILE_SIZE  8192
L4_Word8_t  l4_pmc_logfile_pages[4096 + (IResourcemon_max_cpus * L4_PMC_LOGFILE_SIZE)] __attribute__((aligned(L4_PMC_LOGFILE_SIZE)));
L4_Word8_t *l4_pmc_logfiles[IResourcemon_max_cpus];
L4_Word_t  last_acc_timestamp[IResourcemon_max_cpus];
L4_Word_t  l4_cpu_cnt;
L4_Word_t l4_log_logid;

#if 1
int get_pmclog_other_vms(L4_Word_t cpu, L4_Word64_t from_tsc, L4_Word64_t to_tsc, energy_pmc_t *res_pmc)
{
    unsigned int ctr = 0;
    L4_Word64_t exit_pmc[8], entry_pmc[8];
    L4_Word_t entry_event = 0, exit_event = 0, count = 0;
    
    /*
     * Retrieve log control register and current offset
     */
    L4_LogCtrl_t *c = l4_log_control_regs[cpu];
    L4_Word_t o = (L4_Word_t) (c + (c->X.current_offset / sizeof(L4_LogCtrl_t))) & (L4_LOG_SIZE(c) - 1);
    
    /*
     * offset points into our log file
     */
    volatile L4_Word_t *current_idx = (L4_Word_t *) (l4_pmc_logfiles[cpu] + o);
    
    res_pmc->tsc = res_pmc->uc = res_pmc->mqw = res_pmc->rb =
        res_pmc->mb = res_pmc->mr = res_pmc->mlr = res_pmc->ldm = 0;

	
    if (L4_LOG_ENTRIES(c)==0) {
        //dprintk(0, "No log entries found for CPU %u\n", (int) cpu);
        return 0;	    
    }

    if (L4_LOG_ENTRIES(c)>2048) {
        //dprintk(0, "Too many log entries (%u) found, log corrupt state\n", (int) L4_LOG_ENTRIES(c));
        return 0;
    }

    if (!res_pmc)
    {	
        printk("Res argument invalid\n");
        return 0;
    }

	
    while (count <= (L4_LOG_ENTRIES(c)-32))
    {
	
        /*
         * Logfile of non-running logid contains 8 entry-exit pairs 
         * most recent one must be an entry event
         * read all 8 exit pairs
         */
        for (ctr=0; ctr < 8; ctr++)
        {
            L4_LOG_DEC_LOG(current_idx, c);
            entry_pmc[7-ctr] = *current_idx;
            entry_pmc[7-ctr] <<= 32;
            L4_LOG_DEC_LOG(current_idx, c);
            entry_pmc[7-ctr] += *current_idx;
        }
        entry_event = (L4_Word_t) (entry_pmc[0] & 0x1);; 

        if (entry_event != 1)
        {
            count+=16;
            continue;
        }
	   
		
        /* Reached from tsc? */
        if (entry_pmc[0] < from_tsc)
            break;
	
        /*
         * read all 8 entry pairs 
         */
        for (ctr=0; ctr < 8; ctr++)
        {
            L4_LOG_DEC_LOG(current_idx, c);
            exit_pmc[7-ctr] = *current_idx;
            exit_pmc[7-ctr] <<= 32;
            L4_LOG_DEC_LOG(current_idx, c);
            exit_pmc[7-ctr] += *current_idx;
        }
	
        exit_event = (L4_Word_t) (exit_pmc[0] & 0x1);; 
	
        /* Not yet at to_tsc? */
        if (exit_pmc[0] > to_tsc)
        {
            count += 32;
            continue;
        }
		
        /* Wraparound ? */
        for (ctr=0; ctr < 8; ctr++)
            if (entry_pmc[ctr] < exit_pmc[ctr])
            {
                DUMP_LOGFILE();
                DUMP_COUNTERS();
                printk("wraparound count %d\n", (u32) count);
                continue;
            }
	   
        res_pmc->tsc += entry_pmc[0] - exit_pmc[0];
        res_pmc->uc  += entry_pmc[1] - exit_pmc[1];
        res_pmc->mqw += entry_pmc[2] - exit_pmc[2];
        res_pmc->rb  += entry_pmc[3] - exit_pmc[3];
        res_pmc->mb  += entry_pmc[4] - exit_pmc[4];
        res_pmc->mr  += entry_pmc[5] - exit_pmc[5];
        res_pmc->mlr += entry_pmc[6] - exit_pmc[6];
        res_pmc->ldm += entry_pmc[7] - exit_pmc[7];
        count+=32;
    }
    return count;
}


L4_Word64_t __get_cpu_energy(const int get_idle)
{
    energy_pmc_t hw_pmc, other_vm_pmc, dpmc;
    energy_pmc32_t dpmc32;
    L4_Word64_t energy = 0;
    unsigned int log_count = 0, cpu = L4_ProcessorNo();

    hw_pmc.tsc = hw_pmc.uc = hw_pmc.mqw = hw_pmc.rb =
	hw_pmc.mb = hw_pmc.mr = hw_pmc.mlr = hw_pmc.ldm = 0;
    dpmc.tsc = dpmc.uc = dpmc.mqw = dpmc.rb =
	dpmc.mb = dpmc.mr = dpmc.mlr = dpmc.ldm = 0;
    dpmc32.tsc = dpmc32.uc = dpmc32.mqw = dpmc32.rb =
	dpmc32.mb = dpmc32.mr = dpmc32.mlr = dpmc32.ldm = 0;
    
    rdpmc64(14, hw_pmc.ldm);
    rdpmc64(13, hw_pmc.mr);
    rdpmc64(12, hw_pmc.mb);
    rdpmc64(5, hw_pmc.rb);
    rdpmc64(4, hw_pmc.mqw);
    rdpmc64(1, hw_pmc.mlr);
    rdpmc64(0, hw_pmc.uc);
    rdtsc64(hw_pmc.tsc);
	
    log_count = get_pmclog_other_vms(cpu, last_pmc.tsc, hw_pmc.tsc, &other_vm_pmc);
    
    dpmc.tsc = hw_pmc.tsc - last_pmc.tsc;
    dpmc.uc = hw_pmc.uc - last_pmc.uc; 
    dpmc.mlr = hw_pmc.mlr - last_pmc.mlr; 
    dpmc.mqw = hw_pmc.mqw - last_pmc.mqw; 
    dpmc.rb = hw_pmc.rb - last_pmc.rb; 
    dpmc.mb = hw_pmc.mb - last_pmc.mb; 
    dpmc.mr = hw_pmc.mr - last_pmc.mr; 
    dpmc.ldm = hw_pmc.ldm - last_pmc.ldm; 
    
    if (dpmc.tsc < other_vm_pmc.tsc ||	
	dpmc.uc < other_vm_pmc.uc ||	
	dpmc.mlr < other_vm_pmc.mlr ||	
	dpmc.mqw < other_vm_pmc.mqw ||	
	dpmc.rb < other_vm_pmc.rb ||	
	dpmc.mb < other_vm_pmc.mb ||	
	dpmc.mr < other_vm_pmc.mr ||	
	dpmc.ldm < other_vm_pmc.ldm)	
    {
#if 0
#define DUMP_PMC(p)                             \
        (u32) (dpmc.p),                         \
	    (u32) (other_vm_pmc.p),             \
	    (u32) (dpmc32.p)
	    
        printk("d %d (tdiff %d):\n"
               "\t\t%08u\t%08u\t%08u\n"
               "\t\t%08u\t%08u\t%08u\n"
               "\t\t%08u\t%08u\t%08u\n"
               "\t\t%08u\t%08u\t%08u\n"
               "\t\t%08u\t%08u\t%08u\n"
               "\t\t%08u\t%08u\t%08u\n"
               "\t\t%08u\t%08u\t%08u\n"
               "\t\t%08u\t%08u\t%08u\n",
               (u32) l4_log_logid,
               (u32) (hw_pmc.tsc >> 8),
               DUMP_PMC(tsc), DUMP_PMC(uc), DUMP_PMC(mlr), DUMP_PMC(mqw),
               DUMP_PMC(mlr), DUMP_PMC(mqw), DUMP_PMC(rb), DUMP_PMC(mb),
               DUMP_PMC(mr), DUMP_PMC(ldm));
        L4_KDB_Enter("Bug");
#endif
    }	    
    else
    {

#if defined(ACCOUNT_IDLE)
        dpmc32.tsc = dpmc.tsc;
        dpmc32.tsc /= (resourcemon_shared.max_logid_in_use + 1);
#else
        dpmc32.tsc = dpmc.tsc - other_vm_pmc.tsc;
#endif

        dpmc32.uc = dpmc.uc - other_vm_pmc.uc; 
	dpmc32.mlr = dpmc.mlr - other_vm_pmc.mlr; 
	dpmc32.mqw = dpmc.mqw - other_vm_pmc.mqw; 
	dpmc32.rb = dpmc.rb - other_vm_pmc.rb; 
	dpmc32.mb = dpmc.mb - other_vm_pmc.mb; 
	dpmc32.mr = dpmc.mr - other_vm_pmc.mr; 
	dpmc32.ldm = dpmc.ldm - other_vm_pmc.ldm; 
    }


#if 0
    printk("d %d (tdiff %d):\n"
	   "\t\t%08u\t%08u\t%08u\n"
	   "\t\t%08u\t%08u\t%08u\n"
	   "\t\t%08u\t%08u\t%08u\n"
	   "\t\t%08u\t%08u\t%08u\n"
	   "\t\t%08u\t%08u\t%08u\n"
	   "\t\t%08u\t%08u\t%08u\n"
	   "\t\t%08u\t%08u\t%08u\n"
	   "\t\t%08u\t%08u\t%08u\n",
	   (u32) l4_log_logid,
	   (u32) (hw_pmc.tsc >> 8),
	   DUMP_PMC(tsc), DUMP_PMC(uc), DUMP_PMC(mlr), DUAMP_PMC(mqw),
	   DUMP_PMC(mlr), DUMP_PMC(mqw), DUMP_PMC(rb), DUMP_PMC(mb),
	   DUMP_PMC(mr), DUMP_PMC(ldm));
    L4_KDB_Enter("PMC Record dump");
#endif

    dpmc32.tsc /= 100;
    dpmc32.uc /= 100;
    dpmc32.mqw /= 100;
    dpmc32.rb /= 100;
    dpmc32.mb /= 100;
    dpmc32.mr /= 100;
    dpmc32.mlr /= 100;
    dpmc32.ldm /= 100;

    energy =  
        L4_X86_PMC_UC_WEIGHT * dpmc32.uc +
        L4_X86_PMC_MQW_WEIGHT * dpmc32.mqw +
        L4_X86_PMC_RB_WEIGHT * dpmc32.rb +
        L4_X86_PMC_MB_WEIGHT * dpmc32.mb +
        L4_X86_PMC_MR_WEIGHT * dpmc32.mr +
        L4_X86_PMC_MLR_WEIGHT * dpmc32.mlr +
        L4_X86_PMC_LDM_WEIGHT * dpmc32.ldm;

    if (get_idle)
    {
        energy += L4_X86_PMC_TSC_WEIGHT * dpmc32.tsc;
    }

    //DUMP_ENERGY();
    
    last_pmc.tsc = hw_pmc.tsc;  
    last_pmc.uc  = hw_pmc.uc;   
    last_pmc.mlr = hw_pmc.mlr;
    last_pmc.mqw = hw_pmc.mqw; 
    last_pmc.rb = hw_pmc.rb; 
    last_pmc.mb = hw_pmc.mb;  
    last_pmc.mr = hw_pmc.mr;   
    last_pmc.ldm = hw_pmc.ldm;
		
    return energy;
}

#else
        

static const L4_Word64_t pmc_weight[8] = { L4_X86_PMC_TSC_WEIGHT, 
                                           L4_X86_PMC_UC_WEIGHT,  
                                           L4_X86_PMC_MQW_WEIGHT, 
                                           L4_X86_PMC_RB_WEIGHT, 
                                           L4_X86_PMC_MB_WEIGHT,  
                                           L4_X86_PMC_MR_WEIGHT,  
                                           L4_X86_PMC_MLR_WEIGHT, 
                                           L4_X86_PMC_LDM_WEIGHT };

L4_Word64_t __get_cpu_energy(const int get_idle)
{
    L4_Word64_t most_recent_timestamp = 0;
    L4_Word64_t exit_pmc[8], entry_pmc[8];
    L4_Word64_t energy = 0;
    L4_Word_t entry_event = 0, exit_event;
    L4_Word_t count = 0;
    unsigned int cpu = L4_ProcessorNo();
    
   
    /*
     * Retrieve log control register and current offset
     */        
    
    L4_LogCtrl_t *c = l4_log_control_regs[cpu];
    L4_Word_t o = (L4_Word_t) (c + (c->X.current_offset / sizeof(L4_LogCtrl_t))) & (L4_LOG_SIZE(c) - 1);
    
    /*
     * offset points into our log file
     */
    volatile L4_Word_t *current_idx = (L4_Word_t *) (l4_pmc_logfiles[cpu] + o);
    

   
    if (L4_LOG_ENTRIES(c)==0) {
	printk("No log entries found for CPU %d, skip", cpu);
	return 0;	    
    }

    if (L4_LOG_ENTRIES(c)>2048) 
    {
	printk("Too many log entries (%x) found. Log may be in corrupt state. Skip\n",  
               (u32) L4_LOG_ENTRIES(c));
	return 0;
    }
    
    while (count <= (L4_LOG_ENTRIES(c)-32))
    {
        L4_Word_t ctr, pmc;
	/*
	 * Logfile of non-running logid contains 8 entry-exit pairs 
         * most recent one must be an entry event
	 */
	for (ctr=0; ctr < 8; ctr++)
	{
            L4_LOG_DEC_LOG(current_idx, c);
	    exit_pmc[7-ctr] = *current_idx;
	    exit_pmc[7-ctr] <<= 32;
	    L4_LOG_DEC_LOG(current_idx, c);
	    exit_pmc[7-ctr] += *current_idx;
	}

	exit_event = (L4_Word_t) (exit_pmc[0] & 0x1);
	
        /* Entry event first -- skip */
        if (exit_event == 1)
        {
            count+=16;
            continue;
        }
        else if (count == 0)
        {
            DUMP_LOGFILE();
            DUMP_COUNTERS();
            printk("First entry is exit event count %d\n", (u32) count);
            L4_KDB_Enter("Don't SKIP"); 
        }

	if (most_recent_timestamp == 0)
	{
	    //hout << "timestamp " << (void *) (L4_Word_t) exit_pmc[0] << "\n";
	    most_recent_timestamp = exit_pmc[0];;
	}
        
	/*
	 * Reached stale entries ?
	 */
	if (exit_pmc[0] <= last_acc_timestamp[cpu])
	    break;
	
	/*
	 * read all 8 entry pairs 
	 */
	for (ctr=0; ctr < 8; ctr++)
	{
	    L4_LOG_DEC_LOG(current_idx, c);
	    entry_pmc[7-ctr] = *current_idx;
	    entry_pmc[7-ctr] <<= 32;
	    L4_LOG_DEC_LOG(current_idx, c);
	    entry_pmc[7-ctr] += *current_idx;
	}

	entry_event = (L4_Word_t) (entry_pmc[0] & 0x1);
        
        /* 1 == entry */
	if (entry_event != 1)
	{
            /* log start */
            if (entry_pmc[0] == 0)
                break;
            
            printk("Logfile mismatch entry evt %d/1 logid %d idx %x ct %d sz %d",
                   (u32) entry_event, (u32) l4_log_logid, (u32) current_idx, (u32) count, (u32) L4_LOG_ENTRIES(c));

            DUMP_LOGFILE();
            DUMP_COUNTERS();
            
            L4_KDB_Enter("Mismatch");
	}
        
        /* Wraparound ? */
	if (exit_pmc[0] <= entry_pmc[0])
            break;
        
        
	/*
	 * Estimate energy from counters 
	 */
	for (pmc=1; pmc < 8; pmc++)
            energy += pmc_weight[pmc] * (exit_pmc[pmc] - entry_pmc[pmc]);
        
       
        //DUMP_LOGFILE();
        //DUMP_COUNTERS();
        //printk("energy %x %x\n", (u32) (energy >> 32), (u32) energy);               
        //L4_KDB_Enter("Next PMC");
        count += 32;
    }
    
    do_div(energy, EARM_CPU_DIVISOR * 1000);
    
    //L4_KDB_Enter("Done");
    
    last_acc_timestamp[cpu] = most_recent_timestamp;

    DUMP_ENERGY();
    return energy;
}

#endif
        
L4_Word64_t L4VM_earm_get_cpu_energy(void)
{
	return __get_cpu_energy(1);
}


void L4VM_earm_init(void)
{
    void *kip;
    L4_ThreadId_t myself = L4_Myself(); 
    CORBA_Environment ipc_env = idl4_default_environment;
    int cpu;
    idl4_fpage_t fp;
    int err;
    
    /* get log file */

    kip = L4_GetKernelInterface();
    l4_cpu_cnt = L4_NumProcessors(kip);
    l4_cpu_cnt = (l4_cpu_cnt > IResourcemon_max_cpus) ? IResourcemon_max_cpus : l4_cpu_cnt;
	
    IResourcemon_tid_to_space_id( 
	resourcemon_shared.resourcemon_tid, 
	&myself, &l4_log_logid, &ipc_env );
    
    l4_log_logid += VM_LOGID_OFFSET;
    
    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
	printk("Failed to find out my logid");
	CORBA_exception_free(&ipc_env);
	return;
    }

    /* locate the resource manager */
    err = L4VM_server_locate( UUID_IEarm_Manager, &L4VM_earm_manager_tid );
    if( err ) {
	printk( KERN_ERR "unable to locate the resource manager (%d).\n", UUID_IEarm_Manager );
	return;
    }

	
    printk( KERN_INFO "  EARM logid %d MGR %x\n", (u32) l4_log_logid, (unsigned) L4VM_earm_manager_tid.raw);


    for (cpu=0; cpu < l4_cpu_cnt; cpu++)
    {
        L4_Fpage_t rcvfp;

	l4_log_control_regs[cpu] = &l4_log_control_reg_page[cpu][l4_log_logid];		
	l4_pmc_logfiles[cpu] = &l4_pmc_logfile_pages[cpu * L4_PMC_LOGFILE_SIZE];		

        if (((u32) l4_pmc_logfiles[cpu] & (L4_PMC_LOGFILE_SIZE-1)) != 0)
        {
            l4_pmc_logfiles[cpu] += 4096;
            L4_KDB_Enter("alignment BUG");
        }

        printk( KERN_INFO "  EARM log ctrl regs cpu %u @ %x\n", 
		(u32)  cpu, 
		(u32)  (l4_log_control_reg_page[cpu]));
	printk( KERN_INFO "  EARM log files  cpu %u @ %x (base %x)\n", 
		(u32)  cpu, 
		(u32)  l4_pmc_logfiles[cpu], (u32) l4_pmc_logfile_pages);
        

        
        rcvfp = L4_Fpage((L4_Word_t) l4_log_control_reg_page[cpu], L4_LOG_SELECTOR_SIZE);
	idl4_set_rcv_window(&ipc_env, rcvfp);	
        IResourcemon_request_log_control_regs(resourcemon_shared.resourcemon_tid, cpu, &fp, &ipc_env);

        rcvfp = L4_Fpage((L4_Word_t) l4_pmc_logfiles[cpu],L4_PMC_LOGFILE_SIZE);
        idl4_set_rcv_window(&ipc_env, rcvfp);	
        IResourcemon_request_logfiles(resourcemon_shared.resourcemon_tid, cpu, &fp, &ipc_env);

        

    }

   
}

EXPORT_SYMBOL(L4VM_earm_get_cpu_energy);
EXPORT_SYMBOL(L4VM_earm_manager_tid);
EXPORT_SYMBOL(L4VM_earm_init);



