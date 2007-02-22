#ifndef __L4KA__VT__VM_H__
#define __L4KA__VT__VM_H__


#include <l4/types.h>

#include INC_WEDGE(console.h)
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(module.h)
#include INC_WEDGE(vt/virt_vcpu.h)
#include INC_WEDGE(vt/segdesc.h)	// GDT

/*
 * virtual machine abstraction
 */
class vm_t
{
public:
    // own output console
    hconsole_t vm_con;

    // guest physical address space size
    L4_Word_t gphys_size;

    // kernel module
    module_t *guest_kernel_module;

    L4_Word_t entry_ip;
    
    L4_Word_t ramdisk_start;
    L4_Word_t ramdisk_size;
 
    char *cmdline;
    
    // initialize basic vm parameters
    bool init( L4_ThreadId_t monitor_tid, hiostream_driver_t &con_driver );
    
    // load guest module and prepare vm for starting
    bool init_guest( void ); 

    // create guest thread, guest address space and run guest
    bool start_vm( L4_ThreadId_t monitor );

    // install a GDT with size entries
    bool init_gdt(L4_Word_t base, L4_Size_t size);

    virt_vcpu_t& get_vcpu ( void ) { return this->vcpu; }
    L4_Word_t get_prio() { return this->prio; }

private:
    // load elf file from elf_start
    bool load_elf( L4_Word_t l_elf_start );

    L4_Word_t prio;
    virt_vcpu_t vcpu;
};

INLINE vm_t* get_vm( void ) {
    extern vm_t vm;
    return &vm;
}


#endif // __L4KA__VT__VM_H__
