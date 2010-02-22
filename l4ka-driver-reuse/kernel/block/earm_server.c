/*********************************************************************
 *                
 * Copyright (C) 2009-2010,  Karlsruhe University
 *                
 * File path:     block/earm_server.c
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include <l4/types.h>
#include <l4/arch.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/version.h>

#include <linux/blkdev.h>
#include <linux/buffer_head.h>

#include "server.h"

#define L4VMBLOCK_EARM_UPDATE_BIO_BUDGET_PERIOD	3
#define L4VMBLOCK_EARM_ACCOUNT_PERIOD	        5


#define LD_BIO_RECALC (7)
#define BIO_RECALC    (1UL << LD_BIO_RECALC)

#define ACTIVE_RECALC_THRESHOLD (1)
#define IDLE_RECALC_THRESHOLD   (10)
#define LD_IDLE_RECALC_USEC  (16)
#define IDLE_RECALC_USEC     (1UL << LD_IDLE_RECALC_USEC)

#undef DEBUG_ACC

L4_ThreadId_t earm_disk_tid;
L4_ThreadId_t acc_cpu_tid;
L4_ThreadId_t collector_tid;

L4_Word_t max_uuid_cpu;

/* data shared with the resource monitor */ 
L4VM_alligned_alloc_t L4VMblock_earm_manager_shared_buffer;
IEarm_shared_t *L4VMblock_earm_manager_shared;


L4_Word_t L4VMblock_earm_bio_counter[L4VMBLOCK_MAX_CLIENTS];
L4_Word_t L4VMblock_earm_bio_budget[L4VMBLOCK_MAX_CLIENTS];
L4_Word_t L4VMblock_earm_bio_count, L4VMblock_earm_bio_size;


static struct timer_list update_bio_budget_timer, account_timer;

L4_Word64_t disk_energy_per_byte = 0;

L4_Word_t active_count = 0;
L4_Word_t idle_count = 0;


s64 cpu_unaccounted = 0;
L4_Word64_t cpu_idle_power = 0;         // CPU cost if disk is idle (mW)
L4_Word64_t cpu_bio_energy = 0;         // CPU energy for processing one bio (uJ)

unsigned long cpu_ghz;	

// IBM
// static const L4_Word64_t disk_idle_power = 10200;        // mW
// static const L4_Word64_t disk_active_power = 13500;      // mW
// static const L4_Word64_t disk_avg_seek_time = 3400;      // usec
// static const L4_Word64_t disk_avg_rotation_time = 2000;  // usec
// static const L4_Word_t disk_transfer_rate = 55;        // MB/sec

// Maxtor datasheet
//static const L4_Word64_t disk_idle_power =   (3450);	     // mW
//static const L4_Word64_t disk_active_power = (5600);	     // mW
//static const L4_Word64_t disk_avg_seek_time = 9300;	     // usec
//static const L4_Word64_t disk_avg_rotation_time = 138;     // usec
//static const L4_Word_t   disk_transfer_rate = 57;	     // MB/sec

// Maxtor measured
static const L4_Word64_t disk_idle_power =   (4083);	     // mW
static const L4_Word64_t disk_active_power = (6714);	     // mW
static const L4_Word64_t disk_avg_seek_time = 9300;	     // usec
static const L4_Word64_t disk_avg_rotation_time = 138;     // usec
static const L4_Word_t   disk_transfer_rate = 57;	     // MB/sec



/* client cache */ 
struct client_entry {
  struct list_head list;
  L4_ThreadId_t tid;
  L4_Word_t logid;
};
static LIST_HEAD(clients);
int client_count = 0;

static int tid_to_logid(L4_ThreadId_t tid, L4_Word_t *logid)
{
	L4_Word_t space = 0;
	struct list_head *l;
	struct client_entry *c;
	CORBA_Environment ipc_env;

	// Look for cached entry
	list_for_each(l, &clients) {
		c = list_entry(l, struct client_entry, list);
		if (c->tid.raw == tid.raw) {
			*logid = c->logid;
			return 0;
		}
	}

	// tid not cached, ask resourcemon
	ipc_env = idl4_default_environment;
	IResourcemon_tid_to_space_id( 
		resourcemon_shared.resourcemon_tid, 
		&tid, &space, &ipc_env );
	if( ipc_env._major != CORBA_NO_EXCEPTION ) {
		CORBA_exception_free(&ipc_env);
		printk( KERN_ERR PREFIX "could not get space id\n" );
                L4_KDB_Enter("BUG");
		return -1;
	}
	*logid = space + L4VM_LOGID_OFFSET;

	// cache entry
	c = kmalloc(sizeof(struct client_entry), GFP_KERNEL);
	c->tid = tid;
	c->logid = *logid;
	list_add(&c->list, &clients);
	client_count++;

	return 0;
}


static void account_idle_cost(int idle) 
{
    static L4_Clock_t last = { raw: 0 };
    struct list_head *l;
    struct client_entry *c;
    
    L4_Word64_t cpu_idle_energy, disk_idle_energy;
    L4_Word_t cpu = L4_ProcessorNo();
    L4_Clock_t now = L4_SystemClock();
    L4_Word64_t usec = now.raw - last.raw;

    last = now;    

    if (usec > 0x100000000ULL)
    {
	printk("account_idle_cost: Precision bug while accounting idle power\n");
	return;
    }
    
    /* estimate CPU */
    cpu_idle_energy = cpu_idle_power * usec;
    cpu_unaccounted -= cpu_idle_energy;

    if (idle)
    {
	static L4_Word_t old_bio_count = 0;
	if (L4VMblock_earm_bio_count == old_bio_count)
	{
	    active_count = 0;
	    if (++idle_count > IDLE_RECALC_THRESHOLD)
	    {	
		L4_Word64_t current_idle_power = L4VM_earm_get_cpu_energy();
		//do_div(current_idle_power, usec);
		current_idle_power >>= LD_IDLE_RECALC_USEC;
		cpu_idle_power *= 15;
		cpu_idle_power += current_idle_power;
		cpu_idle_power >>= 4;
		cpu_unaccounted = 0;

#if defined(DEBUG_ACC)
                {
                    static int dbg_cnt = 0;
                    
                    if (dbg_cnt++ % 100 == 0)
                        printk("cpu_idle_power %d now %d old %d, msec %d\n", 
                               (u32) cpu_idle_power, 
                               L4VMblock_earm_bio_count, old_bio_count, (u32) (usec >> 10));
                }
#endif			    
	    }
	    else
                L4VM_earm_get_cpu_energy();

	}
	else
	    idle_count = 0;
	old_bio_count = L4VMblock_earm_bio_count;
    }
    
    disk_idle_energy = disk_idle_power * usec;
    
    /* account idle cost */
    if (client_count > 1)
    {
	do_div(disk_idle_energy, client_count);
	do_div(cpu_idle_energy, client_count);
    }
    list_for_each(l, &clients) {
	c = list_entry(l, struct client_entry, list);
	ASSERT(c->logid < L4_LOG_MAX_LOGIDS);
	L4VMblock_earm_manager_shared->clients[c->logid].base_cost[cpu] += cpu_idle_energy;
	L4VMblock_earm_manager_shared->clients[c->logid].base_cost[UUID_IEarm_ResDisk] += disk_idle_energy;
	
#if defined(DEBUG_ACC)
        {
            static u32 dbg_cnt = 0;
            if (dbg_cnt++ % 100 == 0)
                printk( PREFIX "d %d, msec=%d, disk_idle_power=%d cpu_idle_power=%d, idle_energy=%8d, -> %lu\n",
                        c->logid, ((u32) usec) / 1000, (u32) disk_idle_power, (u32) cpu_idle_power, 
                        (u32) cpu_idle_energy,  ((u32) (L4VMblock_earm_manager_shared->clients[c->logid].base_cost) >> 10));
        }
#endif        
    }

}

void account(unsigned long __unused)
{
    account_idle_cost(1);
    account_timer.expires = jiffies + L4VMBLOCK_EARM_ACCOUNT_PERIOD;
    add_timer(&account_timer);

}

static void calc_bio_energy(L4_Word_t active_usec)
{
#if 0
    L4_Word64_t current_disk_energy_per_byte;
#endif

    L4_Word64_t cpu_energy = L4VM_earm_get_cpu_energy();
    cpu_unaccounted += cpu_energy;
    
   
    cpu_unaccounted -= (cpu_bio_energy << LD_BIO_RECALC);
    account_idle_cost(0);

    if (cpu_unaccounted > 0)
	cpu_bio_energy = cpu_unaccounted >> LD_BIO_RECALC;
    else
	cpu_bio_energy = 0;


    disk_energy_per_byte = (disk_active_power - disk_idle_power) / disk_transfer_rate;

#if 0
    if (++active_count > ACTIVE_RECALC_THRESHOLD)
    {
	current_disk_energy_per_byte = 
	    active_usec * (disk_active_power - disk_idle_power);
	do_div(current_disk_energy_per_byte, L4VMblock_earm_bio_size);
	
	disk_energy_per_byte *= 15;
	disk_energy_per_byte += current_disk_energy_per_byte;
	disk_energy_per_byte >>= 4;
	
	L4VMblock_earm_bio_size = 0;
	
	printk("%d\n", current_disk_energy_per_byte);
	

    }
#endif
}


static void update_bio_budget(unsigned long __server)
{
    int client_idx;
    L4VMblock_client_info_t *client;
    L4VMblock_server_t *server = (L4VMblock_server_t *) __server;
    
    for( client_idx = 0; client_idx < L4VMBLOCK_MAX_CLIENTS; client_idx++ )
    {
	client = L4VMblock_client_lookup( server, client_idx );
	if( client )
	{
	    L4VMblock_earm_bio_counter[client_idx] = 0;
	    L4VMblock_earm_bio_budget[client_idx] = L4VMblock_earm_eas_get_throttle(client->client_space->space_id);
	}
    }
       
    L4VMblock_process_client_io( server );
	
    update_bio_budget_timer.expires = jiffies + L4VMBLOCK_EARM_UPDATE_BIO_BUDGET_PERIOD;
    add_timer(&update_bio_budget_timer);

}



/*****************************************************************
 * Module Iounting
 *****************************************************************/
IDL4_INLINE void  IEarm_Resource_get_client_info_implementation(CORBA_Object  _caller, const L4_Word_t  client_space, 
                                                                idl4_fpage_t * client_config, 
                                                                idl4_fpage_t * server_config, 
                                                                idl4_server_environment * _env)
{
    printk("EARMBLOCK: get client info %x\n",(unsigned) _caller.raw);
    L4_KDB_Enter("UNIMPLEMENTED");
}
IDL4_PUBLISH_IEARM_RESOURCE_GET_CLIENT_INFO(IEarm_Resource_get_client_info_implementation);


/* Interface Iounting::Resource */

IDL4_INLINE void IEarm_Resource_get_counter_implementation(CORBA_Object _caller, L4_Word_t *hi, L4_Word_t *lo,
							      idl4_server_environment *_env)
{
    /* implementation of Iounting::Resource::get_counter */
    L4_Word_t logid, u;
    L4_Word64_t counter = 0;
    
    
    if (tid_to_logid(_caller, &logid) != 0) {
	*hi = *lo =  0;
	printk( PREFIX "get_counter by %x: could not get logid\n", (unsigned) _caller.raw);
	return;
    }

    account_idle_cost(0);

    for (u =0; u <= UUID_IEarm_ResMax; u ++)
    {
	    counter += L4VMblock_earm_manager_shared->clients[logid].base_cost[u];   // disk idle cost
	    counter += L4VMblock_earm_manager_shared->clients[logid].access_cost[u];  // CPU cost
    }
    *hi = (counter >> 32);
    *lo = counter;
    //printk( PREFIX "get_counter by %x: logid=%u, counter=%x %x\n", _caller, logid, (u32) *hi, (u32) *lo);

    return;
}

IDL4_PUBLISH_IEARM_RESOURCE_GET_COUNTER(IEarm_Resource_get_counter_implementation);

void *IEarm_Resource_vtable[IEARM_RESOURCE_DEFAULT_VTABLE_SIZE] = IEARM_RESOURCE_DEFAULT_VTABLE;

void IEarm_Resource_server(void)
{
  L4_ThreadId_t partner;
  L4_MsgTag_t msgtag;
  idl4_msgbuf_t msgbuf;
  long cnt;

  idl4_msgbuf_init(&msgbuf);
  //for (cnt = 0;cnt < IEARM_RESOURCE_STRBUF_SIZE;cnt++)
  //  idl4_msgbuf_add_buffer(&msgbuf, malloc(8000), 8000);

  while (1) {
      partner = L4_nilthread;
      msgtag.raw = 0;
      cnt = 0;

      while (1) {
          idl4_msgbuf_sync(&msgbuf);

          idl4_reply_and_wait(&partner, &msgtag, &msgbuf, &cnt);

          if (idl4_is_error(&msgtag))
            break;

          idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, 
			       IEarm_Resource_vtable[idl4_get_function_id(&msgtag) & IEARM_RESOURCE_FID_MASK]);
      }
  }
}

void IEarm_Resource_discard(void)
{
}


void L4VMblock_earm_end_io(L4_Word_t client_space_id, L4_Word_t size, 
			   L4_Word64_t *disk_energy, L4_Word64_t *cpu_energy)
{
    L4_Word64_t now;
    static L4_Word64_t last = 0;
    L4_Word_t delta;
    L4_Word_t active_usec = 0;
    static s64 usec[BIO_RECALC];
    L4_Word_t logid;
    
    now = get_cycles();
    ASSERT(cpu_ghz);
    delta = (u32) (now - last) / cpu_ghz;
    last = now;
    usec[L4VMblock_earm_bio_count & (BIO_RECALC-1)] = delta;
    if (delta < 10000)
	active_usec += delta;

    L4VMblock_earm_bio_size += size;
    L4VMblock_earm_bio_count++;

    logid = client_space_id + L4VM_LOGID_OFFSET;

    *disk_energy = disk_energy_per_byte * size;
    *cpu_energy = cpu_bio_energy;

    ASSERT(logid < L4_LOG_MAX_LOGIDS);
    ASSERT(L4_ProcessorNo() < UUID_IEarm_ResMax);
    L4VMblock_earm_manager_shared->clients[logid].access_cost[UUID_IEarm_ResDisk] += *disk_energy;
    L4VMblock_earm_manager_shared->clients[logid].access_cost[L4_ProcessorNo()] += *cpu_energy;


#if defined(DEBUG_ACC)
    {   
        static u32 dbg_cnt = 0;

        
        if (dbg_cnt++ % 1000 == 0)
        {
            printk( PREFIX "logid %d cpu_ghz %d, size %d disk_energy=%8u cpu_energy=%8u\n",
                    logid, cpu_ghz, size, (u32) (*disk_energy >> 10), (u32) (*cpu_energy));
        }
    }
#endif
    if ((L4VMblock_earm_bio_count & (BIO_RECALC-1)) == 0) 
    {
#if 0
	L4_Word_t i;
	int avg = 0, var = 0;
	for (i=0; i < BIO_RECALC; i++)
	{
	    avg += usec[i];
	}
	avg >>= LD_BIO_RECALC;
	
	for (i=0; i < BIO_RECALC; i++)
	{
	    var += (avg - usec[i]) * (avg - usec[i]);
	    usec[i] = 0;
	}
	var >>= LD_BIO_RECALC;
#endif	
	//printk("%ld %ld %ld\n", avg, var, active_usec);
	calc_bio_energy(active_usec);
	active_usec = 0;
    }
		
	
}


static void L4VMblock_earm_server(void *dummy) {
    
    CORBA_Environment ipc_env;
    L4_Word_t logid, u;

    /* get the CPU server */
    ipc_env = idl4_default_environment;
    IEarm_Manager_open( L4VM_earm_manager_tid, 0, &acc_cpu_tid, &ipc_env );
    if ( ipc_env._major == CORBA_USER_EXCEPTION ) {
	CORBA_exception_free(&ipc_env);
	printk( KERN_ERR PREFIX "failed to get the CPU server.\n" );
    }

    /* register with the resource manager */
    ipc_env = idl4_default_environment;
    idl4_set_rcv_window( &ipc_env, L4VMblock_earm_manager_shared_buffer.fpage );
    IEarm_Manager_register_resource( L4VM_earm_manager_tid, UUID_IEarm_ResDisk, 
				     (idl4_fpage_t *) &L4VMblock_earm_manager_shared_buffer.fpage, &ipc_env );\
    if ( ipc_env._major != CORBA_NO_EXCEPTION ) {
	L4VM_fpage_dealloc( &L4VMblock_earm_manager_shared_buffer );
	printk( KERN_ERR PREFIX "EARM failed to register with the EARM manager.\n" );
	L4_KDB_Enter("BUG");
    }

    L4VMblock_earm_manager_shared = (IEarm_shared_t *) L4VMblock_earm_manager_shared_buffer.start;

    for (logid = 0; logid < L4_LOG_MAX_LOGIDS; logid++)
    {
	for (u = 0; u < UUID_IEarm_ResMax; u++)
	{
	    L4VMblock_earm_manager_shared->clients[logid].base_cost[u] = 0;
	    L4VMblock_earm_manager_shared->clients[logid].access_cost[u] = 0;
	}
	//L4VMblock_earm_manager_shared->clients[logid].limit = (~0ULL);;
	      
    }

    // Start server loop
    IEarm_Resource_server();
    
    L4_KDB_Enter("BUG Fell through IEarm_Resource_server");
    return;
}


int L4VMblock_earm_init(L4VMblock_server_t *server, L4_Word_t server_irq)
{
    L4_Word_t log2size;
    int err;

    cpu_ghz = (int)L4_ProcDescInternalFreq(
	L4_ProcDesc(
	    L4_GetKernelInterface(), 0) );

    cpu_ghz /= 1000 * 1000;
    
    log2size = L4_SizeLog2( L4_Fpage(0, 16384) );
    err = L4VM_fpage_alloc( log2size, &L4VMblock_earm_manager_shared_buffer );
    if( err < 0 ) 
    {
	printk( KERN_ERR PREFIX "EARM failed to alloc shared fpage");
	L4_KDB_Enter("BUG");
    }
    
 
    earm_disk_tid = L4VM_thread_create( GFP_KERNEL,
					L4VMblock_earm_server, 
					l4ka_wedge_get_irq_prio() -1,
					smp_processor_id(),
					0, 0);

    if( L4_IsNilThread(earm_disk_tid) ) {
	printk( KERN_ERR PREFIX "failed to start the disk server thread.\n" );
	return -1;
    } else {
        printk( KERN_INFO PREFIX "started the disk server thread with tid %x.\n", (unsigned int) earm_disk_tid.raw );
    }


    l4ka_wedge_earm_register_callback(L4VM_earm_manager_tid, UUID_IEarm_ResDisk, server_irq);

#if 1
    init_timer(&update_bio_budget_timer);
    update_bio_budget_timer.function = update_bio_budget;
    update_bio_budget_timer.expires = jiffies + L4VMBLOCK_EARM_UPDATE_BIO_BUDGET_PERIOD;
    update_bio_budget_timer.data = (unsigned long) server;
    update_bio_budget_timer.base = 0;
    update_bio_budget_timer.magic = TIMER_MAGIC;
    update_bio_budget_timer.lock = SPIN_LOCK_UNLOCKED;
    add_timer(&update_bio_budget_timer);
    
    init_timer(&account_timer);
    account_timer.function = account;
    account_timer.expires = jiffies + L4VMBLOCK_EARM_ACCOUNT_PERIOD;
    account_timer.data = 0;
    account_timer.base = 0;
    account_timer.magic = TIMER_MAGIC;
    account_timer.lock = SPIN_LOCK_UNLOCKED;
    add_timer(&account_timer);
    add_timer(&account_timer);
#endif
    
    
    
    
    max_uuid_cpu = L4_NumProcessors(L4_GetKernelInterface()) - 1;
    if (max_uuid_cpu > UUID_IEarm_ResCPU_Max)
	max_uuid_cpu = UUID_IEarm_ResCPU_Max;

    
    return 0;
}



L4_Word_t L4VMblock_earm_eas_get_throttle(L4_Word_t client_space_id)
{	
	L4_Word_t logid = client_space_id + L4VM_LOGID_OFFSET;
	
#if defined(DEBUG_ACC)
	if (logid < 3 || logid > L4_LOG_MAX_LOGIDS)
	    printk("earm_disk_get_throttle_factor: bogus logid %d\n", (int) logid);
        
        {
            static u32 dbg_cnt = 0;
            if (dbg_cnt++ % 1000 == 0)
                if (L4VMblock_earm_manager_shared->clients[logid].limit != ~(0ULL))
                    printk("earm_disk_get_throttle_factor: logid %d limit %u\n", 
                           (u32) logid, (u32) L4VMblock_earm_manager_shared->clients[logid].limit);
        }
#endif
	return L4VMblock_earm_manager_shared->clients[logid].limit;
}
	
	
