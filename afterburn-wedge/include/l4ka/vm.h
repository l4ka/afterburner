/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     l4ka/vm.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/


#ifndef __L4KA__VM_H__
#define __L4KA__VM_H__
#include <l4/types.h>

#include INC_WEDGE(console.h)
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(module.h)

/*
 * virtual machine abstraction
 */
class vm_t
{
public:
    // guest physical address space size
    L4_Word_t gphys_size;

    // kernel module
    module_t *guest_kernel_module;

    L4_Word_t entry_ip;
    L4_Word_t entry_sp;
    
    L4_Word_t ramdisk_start;
    L4_Word_t ramdisk_size;
 
    char *cmdline;
    
    // initialize basic vm parameters
    bool init(word_t ip, word_t sp);
    
    // load guest module and prepare vm for starting
    bool init_guest( void ); 

    L4_Word_t get_prio() { return this->prio; }

private:
    // load elf file from elf_start
    bool load_elf( L4_Word_t l_elf_start );

    L4_Word_t prio;
};

INLINE vm_t* get_vm( void ) {
    extern vm_t vm;
    return &vm;
}



#endif /* !__L4KA__VM_H__ */
