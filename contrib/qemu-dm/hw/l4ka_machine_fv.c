
#include <l4/types.h>
#include <l4/schedule.h>
#include <l4/thread.h>
#include <l4/ipc.h>
#include <l4/message.h>
#include <l4/space.h>
#include <l4/kip.h>

#include "vl.h"
#include <sys/mman.h>
#include <resourcemon_idl_client.h>
#include <qemu-dm_idl_server.h>
#include <qemu-dm_pager_idl_client.h>
#include "ioreq.h"

uint8_t wedge_registered = 0;
extern void *shared_page;

#ifdef CONFIG_L4_PIC_IN_QEMU
//stores the pending irq. The pic only returns the irq vector. However, we need the irq for possible reraises. 
int32_t l4ka_pending_irq = -1UL;
int32_t l4ka_pending_vector = 0;
uint8_t l4ka_is_pending = 0;

uint8_t in_reraise = 0; // are we currently reraising an irq?
#endif

#ifdef __x86_64__
#define PAGE_SHIFT           12
#else
#define PAGE_SHIFT           12
#endif
#define PAGE_SIZE (1UL << PAGE_SHIFT)

#if defined(__i386__) 
#define MAX_MCACHE_SIZE    0x08000000 
#elif defined(__x86_64__)
#define MAX_MCACHE_SIZE    0x08000000 
#endif

#define MAX_MCACHE_ENTRIES (MAX_MCACHE_SIZE >> PAGE_SHIFT)

struct map_cache {
    unsigned long paddr_index;
    uint8_t      *vaddr_base;
};

static struct map_cache mapcache_entry[MAX_MCACHE_ENTRIES];

static uint8_t *map_area;
static L4_ThreadId_t guest_pager;
static L4_ThreadId_t virq_server_id;

/* For most cases (>99.9%), the page address is the same. */
static unsigned long last_address_index = ~0UL;
static uint8_t      *last_address_vaddr;

int idl4_wait_for_event(int timeout);

static inline unsigned long get_vaddr(unsigned long address_index)
{
    return (unsigned long)map_area + (address_index % MAX_MCACHE_ENTRIES) * PAGE_SIZE;
}

static inline uint32_t set_up_guest_mem_area(L4_Word_t start, L4_Word_t size, L4_Word_t type)
{
    CORBA_Environment ipc_env = idl4_default_environment;

    IQEMU_DM_PAGER_Control_set_guest_memory_mapping(guest_pager,start,size,type,&ipc_env);

    if(ipc_env._major != CORBA_NO_EXCEPTION )
    {
        CORBA_exception_free( &ipc_env );
        return -1;
    }

    return 0;
    
}

uint32_t set_up_mmio_area(unsigned long start, unsigned long size)
{
    if(size == 0)
	return set_up_guest_mem_area(start, size, IQEMU_DM_PAGER_INVALID_TYPE );
    return set_up_guest_mem_area(start, size, IQEMU_DM_PAGER_MMIO );
}

static inline int request_page(L4_Word_t guest_paddr, L4_Word_t qemu_vaddr)
{
    CORBA_Environment ipc_env = idl4_default_environment;
    L4_Fpage_t fpage;
    idl4_fpage_t mapping;
    
    fpage = L4_Fpage(qemu_vaddr, PAGE_SIZE);

    idl4_set_rcv_window( &ipc_env, fpage);

    IQEMU_DM_PAGER_Control_request_page(guest_pager,  guest_paddr, &mapping, &ipc_env);

    if(ipc_env._major != CORBA_NO_EXCEPTION )
    {
        CORBA_exception_free( &ipc_env );
        return -1;
    }

    return 0;
    
}

static inline uint8_t *request_special_mem(L4_Word_t type, L4_Word_t size)
{
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE -1);
    uint8_t *start_addr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    if(start_addr == MAP_FAILED) {
	errno = ENOMEM;
	return NULL;
    }

    CORBA_Environment ipc_env = idl4_default_environment;
    L4_Fpage_t fpage;
    idl4_fpage_t mapping;
    
//    printf("cial page with index %lu at start addr %p, offset %lx\n",type,start_addr + i,i);
    //TODO CLEAN up on failure
    for(uint32_t i = 0; i < size; i+=PAGE_SIZE)
    {
	fpage = L4_Fpage((L4_Word_t)start_addr + i, PAGE_SIZE);

	idl4_set_rcv_window( &ipc_env, fpage);

	IQEMU_DM_PAGER_Control_request_special_page(guest_pager,  type, i, &mapping, &ipc_env);

	if(ipc_env._major != CORBA_NO_EXCEPTION )
	{
	    CORBA_exception_free( &ipc_env );
	    return NULL;
	}
    }
    return start_addr;
    
} 
#ifndef CONFIG_L4_PIC_IN_QEMU
void l4ka_raise_irq(unsigned int irq)
{
    CORBA_Environment ipc_env = idl4_default_environment;
    
    // printf("qemu-dm: Attempt to raise irq %d\n",irq);
    // irq mask
    IQEMU_DM_PAGER_Control_raise_irq(guest_pager, irq, &ipc_env);
    
    if(ipc_env._major != CORBA_NO_EXCEPTION )
    {
	CORBA_exception_free( &ipc_env );
	printf("qemu-dm: raising irq %d failed\n",irq);
    }
} 
#else
void l4ka_raise_irq( unsigned int irq)
{
    L4_MsgTag_t tag = L4_Niltag;
    L4_MsgTag_t errtag = L4_Niltag;
    tag.X.u = 2;
    tag.X.label = 0x100; //msg_label_virq

    L4_Set_MsgTag( tag );
    errtag = L4_Send(virq_server_id);
    if(L4_IpcFailed(errtag))
    {
	printf("sending virtual irq failed\n");
	DEBUGGER_ENTER("Untested");
    }
}
#endif


void l4ka_rtc_set_memory(int addr, int val)
{
#ifndef CONFIG_L4_PIC_IN_QEMU
    CORBA_Environment ipc_env = idl4_default_environment;

    IQEMU_DM_PAGER_Control_setCMOSData(guest_pager, addr, val, &ipc_env);
    
    if(ipc_env._major != CORBA_NO_EXCEPTION)
    {
	CORBA_exception_free( &ipc_env );
	printf("qemu-dm: rtc_set_memory failed\n");
    }
#endif
}

L4_ThreadId_t vmctrl_get_root_tid( void )
{
    void *kip = L4_GetKernelInterface();
    return L4_GlobalId( 2+L4_ThreadIdUserBase(kip), 1 );
}


static inline int register_interface( void )
{
    L4_ThreadId_t root_tid = vmctrl_get_root_tid();
    L4_ThreadId_t me = L4_Myself();
    CORBA_Environment env = idl4_default_environment;
    const guid_t guid = UUID_IQEMU_DM;

    IResourcemon_register_interface(root_tid,guid, &me, &env);

    if( env._major != CORBA_NO_EXCEPTION )
    {
        CORBA_exception_free( &env );
        return -1;
    }

    return 0;

}

static inline int send_guest_startup_ipc(void)
{
    CORBA_Environment env = idl4_default_environment;
    L4_ThreadId_t root_tid = vmctrl_get_root_tid();
    IResourcemon_client_init_complete( root_tid, &env );
    if( env._major != CORBA_NO_EXCEPTION )
    {
        CORBA_exception_free( &env );
        return -1;
    }
    return 0;
} 

static inline void unmap_request(L4_Word_t map_area_addr)
{
    L4_Fpage_t fpage = L4_Fpage(map_area_addr,PAGE_SIZE);
    L4_FpageAddRights(fpage, L4_FullyAccessible);
    L4_Flush(fpage);
}

static int qemu_map_cache_init(void)
{
    
    memset(mapcache_entry,0,sizeof(struct map_cache) * MAX_MCACHE_ENTRIES);

    map_area =  mmap(NULL, MAX_MCACHE_SIZE, PROT_READ|PROT_WRITE,
                     MAP_ANON | MAP_SHARED, -1, 0);

    if (mapcache_entry == MAP_FAILED) {
        errno = ENOMEM;
        return -1;
    }

    return 0;
}

static void qemu_remap_bucket(struct map_cache *entry,
                              unsigned long address_index)
{

    unsigned long vaddr = get_vaddr(address_index);
    unsigned long guest_paddr = address_index << PAGE_SHIFT;

    if(request_page(guest_paddr, vaddr)){
	printf("qemu-dm: request_page failed. guest_paddr = %lx, qemu_vaddr = %lx\n",guest_paddr, vaddr);
	entry->paddr_index = 0;
	entry->vaddr_base = NULL;
    }
    else {
	entry->paddr_index = address_index;
	entry->vaddr_base = (uint8_t *)vaddr;
    }
}

uint8_t *qemu_map_cache(target_phys_addr_t phys_addr)
{
    struct map_cache *entry;
    unsigned long address_index  = phys_addr >> PAGE_SHIFT;
    unsigned long address_offset = phys_addr & (PAGE_SIZE-1);

    if (address_index == last_address_index)
        return last_address_vaddr + address_offset;

    entry = &mapcache_entry[address_index % MAX_MCACHE_ENTRIES];

    if (entry->vaddr_base == NULL || entry->paddr_index != address_index)
        qemu_remap_bucket(entry, address_index);

    if (entry->vaddr_base == NULL || entry->paddr_index != address_index)
        return NULL;

    last_address_index = address_index;
    last_address_vaddr = entry->vaddr_base;

    return last_address_vaddr + address_offset;
}

void qemu_invalidate_map_cache(void)
{
    unsigned long i;

    for (i = 0; i < MAX_MCACHE_ENTRIES; i++) {
        struct map_cache *entry = &mapcache_entry[i];

        if (entry->vaddr_base == NULL)
            continue;

        unmap_request((L4_Word_t)entry->vaddr_base);
        entry->paddr_index = 0;
        entry->vaddr_base  = NULL;
    }

    last_address_index =  ~0UL;
    last_address_vaddr = NULL;

}

void *init_vram(unsigned long begin, unsigned long size)
{
    //request vram from memory pager
    printf("request video ram. Size %lx\n",size);
    void * ptr = request_special_mem(IQEMU_DM_PAGER_VMEM, size);

    
    //set up vram mapping
    if(ptr == NULL || set_up_guest_mem_area(begin, size, IQEMU_DM_PAGER_VMEM))
    {
	printf("setting up videa memory area failed. start %lx, size %lx\n",begin,size);
	exit(-1);
    }
       
    return ptr;
    
}

void destroy_vram(unsigned long begin, unsigned long size, void * mapping)
{
    L4_Fpage_t fpage;
    //request deletion of guest memory area starting at begin
    if(set_up_guest_mem_area(begin, 0, IQEMU_DM_PAGER_INVALID_TYPE))
    {
	printf("deleting guest memory area failed. start %lx, size %lx\n",begin,size);
	exit(-1);
    }

    if(munmap(mapping, size))
    {
	printf("munmap failed with error code %d\n",errno);
    }

    for(uint32_t i = 0; i < size; i += PAGE_SIZE)
    {
	fpage = L4_Fpage((begin + i) & ~(PAGE_SIZE -1),PAGE_SIZE);
	L4_UnmapFpage(fpage);
    }
}

static void l4ka_init_fv(uint64_t ram_size, int vga_ram_size, char *boot_device,
                        DisplayState *ds, const char **fd_filename,
                        int snapshot,
                        const char *kernel_filename,
                        const char *kernel_cmdline,
                        const char *initrd_filename,
                        const char *direct_pci)
{

    printf("Initialize L4ka fully virtualized PC machine\n");
    printf("QEMU-DM thread id: %x\n", L4_Myself());

    if (qemu_map_cache_init()) {
        printf("qemu: qemu_map_cache_init() returned: error %d\n", errno);
        exit(-1);
    }

#ifndef CONFIG_L4_PIC_IN_QEMU
    init_irq_logic();
#endif

    printf("Register Qemu-dm interface\n");
    if(register_interface())
	printf("qemu: Failed to register Qemu-dm interface\n");

    printf("Send Guest startup ipc\n");
    if(send_guest_startup_ipc())
    {
	printf("Sending Guest startup ipc failed\n");
	exit(-1);
    }


    printf("Wait for afterburner wedge to register\n");
    while(!wedge_registered)
	idl4_wait_for_event(0);

    printf("Afterburner registered with Memory pager id %lx\n",guest_pager.raw);

    shared_page = request_special_mem( IQEMU_DM_PAGER_SHARED_IO_PAGE, PAGE_SIZE);
    if(!shared_page)
    {
	printf("qemu: requesting shared_iopage failed\n");
	exit(-1);
    }

    //set up frame buffer area
    if(set_up_guest_mem_area(0xA0000, 0x20000 , IQEMU_DM_PAGER_MMIO))
    {
	printf("qemu: setting up guest framebuffer memory area  failed\n");
	exit(-1);
    }

    pc_machine.init(ram_size, vga_ram_size, boot_device, ds, fd_filename,
                    snapshot, kernel_filename, kernel_cmdline, initrd_filename,
                    direct_pci);
    printf("Initializing L4ka machine completed\n");
}

QEMUMachine l4ka_fv_machine = {
    "L4ka_fv",
    "L4ka Fully-virtualized PC",
    l4ka_init_fv,
};


static inline void __wait(L4_ThreadId_t *partner, L4_MsgTag_t *msgtag, idl4_msgbuf_t *msgbuf, L4_Time_t rcvTimeout)
{
    L4_MsgTag_t _result;

    _result = L4_Wait_Timeout(rcvTimeout, partner);

    L4_MsgStore(_result, (L4_Msg_t*)msgbuf);
    *msgtag = _result;
}

void __reply(L4_ThreadId_t *partner, L4_MsgTag_t *msgtag, idl4_msgbuf_t *msgbuf)
{
    L4_MsgTag_t _result;

#if defined(L4_COMPAT_H_LOCATION)
  L4_Reset_WordSizeMask();
#endif

    L4_MsgLoad((L4_Msg_t*)&msgbuf->obuf);
    _result = L4_Reply(*partner);
    *msgtag = _result;
}


/* Interface IQEMU_DM::Control */

IDL4_INLINE void  IQEMU_DM_Control_register_implementation(CORBA_Object  _caller, const IQEMU_DM_threadId_t  qemu_pager, const IQEMU_DM_threadId_t irq_server,  idl4_server_environment * _env)

{
    guest_pager.raw = qemu_pager;
    virq_server_id.raw = irq_server;
    wedge_registered = 1;

    return;
}

IDL4_PUBLISH_IQEMU_DM_CONTROL_REGISTER(IQEMU_DM_Control_register_implementation);

IDL4_INLINE void  IQEMU_DM_Control_raiseEvent_implementation(CORBA_Object  _caller, const IQEMU_DM_event_t  event, idl4_server_environment * _env)

{
    switch(event)
    {
	case IQEMU_DM_EVENT_IO_REQUEST:
	{
	    extern CPUState *cpu_single_env;
	    extern void cpu_handle_ioreq(void *opaque);
	    cpu_handle_ioreq(cpu_single_env);
	    break;
	}
	case IQEMU_DM_EVENT_SUSPEND_REQUEST:
	{
	    printf("qemu-dm: Suspend request not implemented");
	    break;
	}
	default:
	    CORBA_exception_set(_env, ex_IQEMU_DM_invalid_event, NULL);
    }
}

IDL4_PUBLISH_IQEMU_DM_CONTROL_RAISEEVENT(IQEMU_DM_Control_raiseEvent_implementation);

void * IQEMU_DM_Control_vtable[IQEMU_DM_CONTROL_DEFAULT_VTABLE_SIZE] = IQEMU_DM_CONTROL_DEFAULT_VTABLE;

int idl4_wait_for_event(int timeout)
{

    static L4_ThreadId_t  partner;
    static L4_MsgTag_t  msgtag;
    static idl4_msgbuf_t  msgbuf;
    static long  cnt;
    static int isInit = 0;
    if(!isInit)
    {
	idl4_msgbuf_init(&msgbuf);
	partner = L4_anythread;
	msgtag.raw = 0;
	cnt = 0;
	isInit = 1;
    }

    L4_Time_t rcvTimeout = L4_TimePeriod(timeout);

    __wait(&partner, &msgtag, &msgbuf, rcvTimeout);

    if (idl4_is_error(&msgtag))
    {
	partner = L4_anythread;
	msgtag.raw = 0;
	cnt = 0;
	// ignore timeout errors
	return (L4_ErrorCode() == 3) ? 0 : -1;
    }

    idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, IQEMU_DM_Control_vtable[idl4_get_function_id(&msgtag) & IQEMU_DM_CONTROL_FID_MASK]);

    idl4_msgbuf_sync(&msgbuf);

    __reply(&partner, &msgtag, &msgbuf); 

    return 0;
}

void  IQEMU_DM_Control_discard()
{
}
