#ifndef RESOURCEMON_H_
#define RESOURCEMON_H_
#if !defined(NULL)
#define NULL (0)
#endif

#include <l4/types.h>

namespace l4app
{
	
class VMInfo
{
public:
	L4_Word_t get_space_size(void) const { return space_size; }
	
private:  
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
};

class Resourcemon
{
public:
   Resourcemon(L4_ThreadId_t resourcemonTid =  L4_nilthread)
	: m_resourcemonTid(resourcemonTid) {}

	int getVMInfo(L4_Word_t spaceId, VMInfo& vmInfo) ;
	int getVMSpace(L4_Word_t spaceId, char*& vmSpace) ;
	int deleteVM(L4_Word_t spaceId) ;
	int reserveVM(VMInfo& vmInfo, L4_Word_t *spaceId) ;
	int mapVMSpace(L4_Word_t spaceId, const char *vmSpace) ;

	inline L4_ThreadId_t getResourcemonTid() {
		return m_resourcemonTid;
	}
	
private:
	L4_ThreadId_t m_resourcemonTid;
};

}; // namespace l4app

#endif /*RESOURCEMON_H_*/
