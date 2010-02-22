/*********************************************************************
 *                
 * Copyright (C) 2009-2010,  Karlsruhe University
 *                
 * File path:     block/earm_client.c
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include <l4/types.h>
#include <l4/arch.h>
#include <l4/kip.h>

#include <l4/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/binfmts.h>

#if defined(CONFIG_RC_DISK)
#include <linux/rc_task.h>
#include <linux/rc_driver.h>
#endif

#define PREFIX "L4VMblock client: "

#include "client.h"


static L4_ThreadId_t disk_tid;
static L4_Word_t logid;
static  L4_Word_t energy_hi = 0, energy_lo = 0;

#if defined(EARM_BLOCK_CLIENT_DEBUG)

extern int access_process_vm(struct task_struct *tsk, unsigned long addr, void *buf, int len, int write);
static void get_task_cmdline(struct task_struct *task, char * buffer)
{
	int res = 0;
	unsigned int len;
	struct mm_struct *mm = get_task_mm(task);
	if (!mm)
	    return;

 	len = mm->arg_end - mm->arg_start;
 
	if (len > PAGE_SIZE)
		len = PAGE_SIZE;
 
	res = access_process_vm(task, mm->arg_start, buffer, len, 0);

	// If the nul at the end of args has been overwritten, then
	// assume application is using setproctitle(3).
	if (res > 0 && buffer[res-1] != '\0') {
		len = strnlen(buffer, res);
		if (len < res) {
		    res = len;
		} else {
			len = mm->env_end - mm->env_start;
			if (len > PAGE_SIZE - res)
				len = PAGE_SIZE - res;
			res += access_process_vm(task, mm->env_start, buffer+res, len, 0);
			res = strnlen(buffer, res);
		}
	}
	mmput(mm);
}

#if defined(CONFIG_RC)
static inline void debug_rc(struct task_struct *t, L4_Word_t usec) {
  struct rc *rc;
  L4_Word_t cpu_energy, disk_energy, disk_limit;
  static L4_Word_t cpu_last = 0, disk_last = 0;

  rc = rc_task_get_rc(&t->trc);
  BUG_ON(!rc);
  cpu_energy = rc->res[RES_CPU].used - cpu_last;
  cpu_last = rc->res[RES_CPU].used;
  
  disk_energy = rc->res[RES_DISK].used - disk_last;
  disk_last = rc->res[RES_DISK].used;
  
  disk_limit = rc->res[RES_DISK].limit[0] * HZ /
      (rc_driver_time_slice_length(0) * usec);
  L4_printk(" %d: [c%u] [d%u / %u]\n", 
	    t->pid, 
	    cpu_energy / usec, 
	    disk_energy / usec, 
	    disk_limit);
  
  rc_put(rc);
}
#endif

static void earm_disk_client_thread(void *unused) {
    
    CORBA_Environment ipc_env = idl4_default_environment;
    L4_Time_t sleep = L4_TimePeriod( EARM_BLOCK_CLIENT_MSEC * 1000 );
    IEarm_energy_t energy;

    sleep = L4_TimePeriod( EARM_BLOCK_CLIENT_MSEC * 1000 );

    for (;;) {

		
        ipc_env = idl4_default_environment;
        IEarm_Resource_get_counter(disk_tid, &energy_hi, &energy_lo, &ipc_env);
		
        if( ipc_env._major != CORBA_NO_EXCEPTION ) {
            CORBA_exception_free(&ipc_env);
            printk( PREFIX "unable to get disk energy.\n" );
            L4_KDB_Enter("");
        } 
		
        energy = energy_hi;
        energy <<= 32;
        energy |= energy_lo;
		
        {
            L4_Clock_t time = L4_SystemClock();
            static L4_Clock_t old_time = { raw: 0 };
            L4_Word64_t usec = time.raw - old_time.raw;
            old_time = time;
            L4_Word64_t old_energy;
            if (energy != old_energy) 
            {
                L4_Word64_t diff = energy - old_energy;
                do_div(diff, usec);
                printk( PREFIX "cumulative disk diff: %u\n", (u32) (diff) );
                old_energy = energy;
            }
#if defined(CONFIG_RC)
            {
                struct rc *rc;
                struct task_struct *root = 0;
                struct task_struct *t;
                struct list_head *l;
                char cmd[256];
                L4_Word_t cpu_energy = 0, old_cpu_energy = 0, disk_energy = 0, old_disk_energy = 0;
                L4_Word_t cpu_limit = 0, disk_limit = 0;

                if (!root) {
                    for_each_process(t) {
                        if (t->pid > 250) {
                            get_task_cmdline(t, (char *) &cmd);
//		    printk("%d: %s\n", t->pid, cmd);
		    
                            if (!strcmp(cmd, "-bash")) {
                                root = t;
                                printk("found bash %d: %s\n", t->pid, cmd);
                                break;
                            }
                        }
                    }
                } else {
                    L4_Word_t usec32 = usec;
                    L4_Word_t msec32 = usec32 / 1000;
			
                    //debug_rc(root, usec);
                    list_for_each(l, &root->children) {
                        t = list_entry(l, struct task_struct, sibling);
                        get_task_cmdline(t, (char *) &cmd);

                        rc = rc_task_get_rc(&t->trc);
                        BUG_ON(!rc);
                        cpu_energy = rc->res[RES_CPU].used;
                        cpu_energy = cpu_energy - old_cpu_energy;
                        old_cpu_energy = rc->res[RES_CPU].used;
                        cpu_limit = rc->res[RES_CPU].limit[0] * HZ /
                            rc_driver_time_slice_length(0);
                        cpu_limit /= msec32;
		
		
                        disk_energy = rc->res[RES_DISK].used - old_disk_energy;
                        old_disk_energy = rc->res[RES_DISK].used;
                        disk_limit = rc->res[RES_DISK].limit[0] * HZ /
                            rc_driver_time_slice_length(0);
                        disk_limit /= usec32;
		
                        if (logid == 4)
                            printk("%d: [c%u / %u] [d%u / %u]\n", 
                                   t->pid, 
                                   cpu_energy / msec32, 
                                   cpu_limit,
                                   disk_energy / usec32,  
                                   disk_limit);
                        else
                            printk("t%d: (c%u / %u) (d%u / %u)\n", 
                                   t->pid, 
                                   cpu_energy / msec32, 
                                   cpu_limit,
                                   disk_energy / usec32,  
                                   disk_limit);
                        rc_put(rc);
                    }
                }
            }
#endif /* defined(CONFIG_RC) */
        }
        
        L4_Sleep(sleep);
    };

    L4_KDB_Enter("earm_disk_client_thread fell through");
}
#endif /* defined(EARM_BLOCK_CLIENT_DEBUG) */

void earm_disk_client_init(void)
{
    L4_ThreadId_t res_manager_tid;
    CORBA_Environment ipc_env;
    unsigned long irq_flags;
#if defined(EARM_BLOCK_CLIENT_DEBUG)
    L4_ThreadId_t earm_disk_tid;
#endif
    L4_ThreadId_t myself = L4_Myself(); 


    /* locate the resource manager */
    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IResourcemon_query_interface( 
        resourcemon_shared.locator_tid, 
        UUID_IEarm_Manager, &res_manager_tid, &ipc_env );
    local_irq_restore(irq_flags);
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
        CORBA_exception_free(&ipc_env);
        printk( KERN_ERR PREFIX "unable to locate the resource manager.\n" );
        L4_KDB_Enter();
        return;
    }

    /* get the disk server */
    ipc_env = idl4_default_environment;
    local_irq_save(irq_flags);
    IEarm_Manager_open( res_manager_tid, UUID_IEarm_ResDisk, &disk_tid, &ipc_env );
    local_irq_restore(irq_flags);
    if ( ipc_env._major == CORBA_USER_EXCEPTION ) {
        CORBA_exception_free(&ipc_env);
        printk( KERN_ERR PREFIX "failed to get the disk server.\n" );
        L4_KDB_Enter();
    }
    printk( KERN_INFO PREFIX "located disk server at %x\n", (unsigned int) disk_tid.raw );

    ipc_env = idl4_default_environment;
    IResourcemon_tid_to_space_id( 
        resourcemon_shared.resourcemon_tid, 
        &myself, &logid, &ipc_env );
  
    if( ipc_env._major != CORBA_NO_EXCEPTION )
    {
        L4_KDB_Enter("Failed to find out my logid id");
        CORBA_exception_free(&ipc_env);
        return;
    }

    logid += L4VM_LOGID_OFFSET;  	

    ipc_env = idl4_default_environment;
    IEarm_Resource_get_counter(disk_tid, &energy_hi, &energy_lo, &ipc_env);
		
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
        CORBA_exception_free(&ipc_env);
        printk( PREFIX "unable to get disk energy.\n" );
    } 
		


#if defined(EARM_BLOCK_CLIENT_DEBUG)
    /* start kernel thread */
    earm_disk_tid = L4VM_thread_create( GFP_KERNEL,
					earm_disk_client_thread, 
                                        l4ka_wedge_get_irq_prio(), 
					smp_processor_id(),
					(void *) 0, 
					0);
    if( L4_IsNilThread(earm_disk_tid) ) {
        printk( KERN_ERR PREFIX "Could not start accounting thread\n" );
        L4_KDB_Enter();
    } 
    else 
    {
        //    conn->accounting_tid = tid;
        printk( KERN_INFO PREFIX "Start disk accounting: tid=%x\n", (unsigned int) earm_disk_tid.raw );
    }
#endif    
    
}
