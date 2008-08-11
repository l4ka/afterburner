
#include <l4/types.h>
#include <l4/schedule.h>
#include <l4/thread.h>
#include <l4/ipc.h>
#include <l4/message.h>
#include <l4/space.h>

#include "vl.h"
#include <sys/mman.h>
#include <qemu-dm_idl_server.h>

uint8_t *guest_phy_mem_start;
size_t phy_mem_size = 0;

size_t map_area_size;
uint8_t *map_area_start;

static int qemu_create_map_area(void)
{
    /*
     * shared_iopage, buffered_iopage, buffered_piopage, free page GUEST phyiscal memory mapping 
     * |_________________________________________________________| |___________________________|
     *                   first map item                                second map item
     */
    map_area_size = IQEMU_DM_max_physical_memory + (PAGE_SIZE * 4);

    map_area_start = mmap(NULL, phy_mem_size, PROT_READ|PROT_WRITE,
                          MAP_ANON, -1, 0);
    if (map_area_start == MAP_FAILED) {
        errno = ENOMEM;
        return -1;
    }

    return 0;
}

uint8_t *qemu_map_cache(target_phys_addr_t phys_addr)
{
    if(phys_addr >= phy_mem_size)
	return NULL;

    return  guest_phy_mem_start + (uint8_t *)phys_addr;
}

void qemu_invalidate_map_cache(void)
{
    
}

static inline void set_rcv_window(L4_Word_t start_addr, size_t size)
{
    L4_Fpage_t rcv_window = L4_Fpage(start_addr,size);
    L4_Acceptor_t  rcv_acpt = L4_MapGrantItems(rcv_window);
    L4_Accept(rcv_acpt);
}

static void l4ka_init_fv(uint64_t ram_size, int vga_ram_size, char *boot_device,
                        DisplayState *ds, const char **fd_filename,
                        int snapshot,
                        const char *kernel_filename,
                        const char *kernel_cmdline,
                        const char *initrd_filename,
                        const char *direct_pci)
{
    extern void *shared_page;
    extern void *buffered_io_page;
    extern void *buffered_pio_page; //unused in i386

    ipc_env = idl4_default_environment;
    
    if (qemu_create_map_area()) {
        printf("qemu_create_map_area returned: error %d\n", errno);
        exit(-1);
    }

    set_rcv_window(map_area_start, map_area_size);
    
    printf("l4ka_init_fv: map_area_start = %x, map_area_size = %x\n",
	   map_area_start, map_area_size);

    idl4_set_rcv_window( &ipc_env, fpage);

    pc_machine.init(ram_size, vga_ram_size, boot_device, ds, fd_filename,
                    snapshot, kernel_filename, kernel_cmdline, initrd_filename,
                    direct_pci);
}

QEMUMachine l4ka_fv_machine = {
    "L4ka_fv",
    "L4ka Fully-virtualized PC",
    l4ka_init_fv,
};

static inline void __idl4_wait(L4_ThreadId_t *partner, L4_MsgTag_t *msgtag, idl4_msgbuf_t *msgbuf, L4_Time rcvTimeout)
{
    L4_MsgTag_t _result;

    _result = L4_Wait(rcvTimeout,partner);

    L4_MsgStore(_result, (L4_Msg_t*)msgbuf);
    *msgtag = _result;
}

static inline void __idl4_reply(L4_ThreadId_t *partner, L4_MsgTag_t *msgtag, idl4_msgbuf_t *msgbuf)
{
    L4_MsgTag_t _result;

    L4_MsgLoad((L4_Msg_t*)&msgbuf->obuf);
    _result = L4_Reply(*partner);
    *msgtag = _result;
}


/* Interface IQEMU_DM::Control */

IDL4_INLINE void  IQEMU_DM_Control_register(CORBA_Object  _caller, const idl4_fpage_t * shared_iqreq_pages, const idl4_fpage_t * guest_physical_memory, const IQEMU_DM_size_t  pmem_size, const IQEMU_DM_threadId_t  softInterruptId, idl4_server_environment * _env)

{
  /* implementation of IQEMU_DM::Control::register */
  
  return;
}

IDL4_PUBLISH_IQEMU_DM_CONTROL_REGISTER(IQEMU_DM_Control_register_implementation);

IDL4_INLINE void  IQEMU_DM_Control_raiseEvent_implementation(CORBA_Object  _caller, idl4_server_environment * _env)

{
  /* implementation of IQEMU_DM::Control::raiseEvent */
  
  return;
}

IDL4_PUBLISH_IQEMU_DM_CONTROL_RAISEEVENT(IQEMU_DM_Control_raiseEvent_implementation);



void * IQEMU_DM_Control_vtable[IQEMU_DM_CONTROL_DEFAULT_VTABLE_SIZE] = IQEMU_DM_CONTROL_DEFAULT_VTABLE;


void  idl4_wait_for_event(L4_ThreadId_t *partner, L4_MsgTag_t *msgtag, idl4_msgbuf_t *msgbuf, long *cnt, int timeout)
{

    L4_Time_t rcvTimeout = L4_TimePeriod(timeout);
    if(msgbuf != NULL)
	idl4_msgbuf_sync(msgbuf);

    __idl4_wait(partner, msgtag, msgbuf, rcvTimeout);

    if (idl4_is_error(&msgtag))
	return;

    idl4_process_request(&partner, &msgtag, &msgbuf, &cnt, IQEMU_DM_Control_vtable[idl4_get_function_id(&msgtag) & IQEMU_DM_CONTROL_FID_MASK]);

    idl4_msgbuf_sync(msgbuf);

    __idl4_reply(partner, msgtag, msgbuf);

    //TODO check msgtag for error

}
