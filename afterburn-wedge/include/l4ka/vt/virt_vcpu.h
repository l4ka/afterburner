#ifndef L4KAVT__VCPU_H__
#define L4KAVT__VCPU_H__

#include INC_WEDGE(debug.h)
#include INC_WEDGE(iostream.h)

#include INC_WEDGE(vt/ia32.h)
#include <hiostream.h>

#include <l4/arch.h>
#include <l4/types.h>
#include <l4/kdebug.h>

class vm_t;

class virt_vcpu_t
{
public:
    bool init( L4_ThreadId_t tid, L4_ThreadId_t monitor_tid, vm_t *pvm );
    L4_ThreadId_t get_thread_id() { return this->thread_id; }
    vm_t* get_vm() { return this->vm; }
    
    bool send_startup_ipc( L4_Word_t ip, bool rm );

    bool process_vfault_message();
    
    bool deliver_interrupt();

    L4_ThreadId_t irq_gtid, irq_ltid;
    L4_ThreadId_t monitor_gtid, monitor_ltid;

    enum runstate_e {
	RUNNING,
	WAITING_FOR_INTERRUPT,
    } runstate;

private:
    bool handle_register_write();
    bool handle_register_read();
    bool handle_instruction();
    bool handle_exception();
    bool handle_bios_call();
    bool handle_io_write();
    bool handle_io_read();
    bool handle_msr_write();
    bool handle_msr_read();
    bool handle_unknown_msr_write();
    bool handle_unknown_msr_read();
    bool handle_interrupt( bool set_ip = false );

    bool read_from_disk( u8_t *ramdisk_start, word_t ramdisk_size, word_t sector_start, word_t sectors, word_t buf_addr );

    L4_Word_t get_ip();
    L4_Word_t get_instr_len();

private:
    vm_t *vm;
    L4_ThreadId_t thread_id;
    bool wait_for_interrupt_window_exit;
    L4_Word_t resume_ip;

    hiostream_t con;
    hiostream_kdebug_t con_driver;
};

#endif /* L4KAVT__VCPU_H__ */
