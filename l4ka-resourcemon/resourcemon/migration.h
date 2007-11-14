#ifndef MIGRATION_H_
#define MIGRATION_H_

#include <l4/types.h>

//vm_t *do_clone_vm(L4_Word_t source_id);

typedef struct _VMInfo {
	// general VM info
	
	// contiguous physical memory region (paddr_len)
	L4_Word_t space_size;
   	// The base address for this VM's contiguous memory region within the 
    // resourcemon
    L4_Word_t haddr_base;  
	// virtual address space offset
	L4_Word_t vaddr_offset;

	// wedge memory information
	L4_Word_t wedge_paddr; // physical start address
    L4_Word_t wedge_vaddr; // virtual start address
    L4_Word_t wedge_size; // size
	
	// shared data between the resourcemon and the VM
	// resides in the resourcemon's physical memory and
	// is mapped into the VM address space, so this area
	// must be copied separately
	static const L4_Word_t client_shared_size = 4096;
    L4_Word_t client_shared_area[client_shared_size];
    // client shared page vaddr from ELF
    L4_Word_t client_shared_vaddr;

	// KIP, as read from the ELF image
	// L4_Fpage_t source_kip_fp = source_vm->get_kip_fp();
	// L4_Word_t kip_start = L4_Address(source_kip_fp);
	// L4_Word_t kip_size = L4_Size(source_kip_fp);
	// this->kip_fp = L4_Fpage(kip_start, kip_size);
	L4_Word_t kip_start;
	L4_Word_t kip_size;

    // UTCB, as read from the ELF image
    // L4_Fpage_t source_utcb_fp = source_vm->get_utcb_fp();
    // L4_Word_t utcb_start = L4_Address(source_utcb_fp);
    // L4_Word_t utcb_size = L4_Size(source_utcb_fp);
    // this->utcb_fp = L4_Fpage(utcb_start, utcb_size);	
	L4_Word_t utcb_start;
	L4_Word_t utcb_size;

	// start and stack addresses
	L4_Word_t binary_stack_vaddr;
	L4_Word_t binary_entry_vaddr;

	// energy accounting info
} VMInfo;

#endif /*MIGRATION_H_*/
