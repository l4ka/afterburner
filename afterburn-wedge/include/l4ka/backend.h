/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/backend.h
 * Description:   
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4KA__BACKEND_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4KA__BACKEND_H__

#include INC_ARCH(cpu.h)
#include INC_ARCH(intlogic.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(user.h)

#include <device/pci.h>
#include <elfsimple.h>

extern thread_info_t * backend_handle_pagefault( L4_MsgTag_t tag, L4_ThreadId_t tid );
extern bool backend_sync_deliver_vector( L4_Word_t vector, bool old_int_state, bool use_error_code, L4_Word_t error_code );
extern bool backend_async_irq_deliver( intlogic_t &intlogic );

extern void NORETURN backend_handle_user_vector( word_t vector );

extern void backend_interruptible_idle( burn_redirect_frame_t *frame );
extern void backend_activate_user( iret_handler_frame_t *iret_emul_frame );

extern void backend_invalidate_tlb( void );
extern void backend_enable_paging( word_t *ret_address );
extern void backend_flush_vaddr( word_t vaddr );
extern void backend_flush_user( void );
extern void backend_flush_page( u32_t paddr );
extern void backend_flush_superpage( u32_t paddr );
extern pgent_t * backend_resolve_addr( word_t user_vaddr, word_t &kernel_vaddr);

INLINE void backend_write_msr( word_t msr_addr, word_t lower, word_t upper )
{
    con << "Unhandled msr write.\n";
}

INLINE void backend_protect_fpu()
{
}

INLINE word_t backend_phys_to_virt( word_t paddr )
{
    return paddr + get_vcpu().get_kernel_vaddr();
}

extern bool backend_enable_device_interrupt( u32_t interrupt, vcpu_t &vcpu);
extern bool backend_disable_device_interrupt( u32_t interrupt, vcpu_t &vcpu);
extern bool backend_unmask_device_interrupt( u32_t interrupt );
extern u32_t backend_get_nr_device_interrupts( void );

struct backend_vcpu_init_t
{
    L4_Word_t entry_sp;
    L4_Word_t entry_ip;
    L4_Word8_t *entry_param;
    L4_Word_t boot_id;
    L4_Bool_t vcpu_bsp;
};

extern bool backend_load_vcpu( backend_vcpu_init_t *init_info );
extern bool backend_preboot( backend_vcpu_init_t *init_info );

extern void backend_reboot( void );

extern void backend_exit_hook( void *handle );
extern int  backend_signal_hook( void *handle );

extern "C" word_t __attribute__((regparm(1))) backend_pte_read_patch( pgent_t *pgent );
extern "C" word_t __attribute__((regparm(1))) backend_pgd_read_patch( pgent_t *pgent );
extern "C" void __attribute__((regparm(2))) backend_pte_write_patch( pgent_t new_val, pgent_t *old_pgent );
extern "C" void __attribute__((regparm(2))) backend_pte_or_patch( word_t bits, pgent_t *old_pgent );
extern "C" void __attribute__((regparm(2))) backend_pte_and_patch( word_t bits, pgent_t *old_pgent );
extern "C" void __attribute__((regparm(2))) backend_pgd_write_patch( pgent_t new_val, pgent_t *old_pgent );
extern "C" word_t __attribute__((regparm(2))) backend_pte_test_clear_patch( word_t bit, pgent_t *pgent );
extern "C" word_t __attribute__((regparm(2))) backend_pte_xchg_patch( pgent_t new_val, pgent_t *pgent );


#if defined(CONFIG_GUEST_PTE_HOOK) || defined(CONFIG_VMI_SUPPORT)
extern void backend_set_pte_hook( pgent_t *old_pte, pgent_t new_pte, int level);
extern word_t backend_pte_test_and_clear_hook( pgent_t *pgent, word_t bit );
extern pgent_t backend_pte_get_and_clear_hook( pgent_t *pgent );
#endif
#if defined(CONFIG_GUEST_UACCESS_HOOK)
extern word_t backend_get_user_hook( void *to, const void *from, word_t n );
extern word_t backend_put_user_hook( void *to, const void *from, word_t n );
extern word_t backend_copy_from_user_hook( void *to, const void *from, word_t n );
extern word_t backend_copy_to_user_hook( void *to, const void *from, word_t n );
extern word_t backend_clear_user_hook( void *to, word_t n );
extern word_t backend_strnlen_user_hook( const char *s, word_t n );
extern word_t backend_strncpy_from_user_hook( char *dst, const char *src, word_t count, word_t *success );
#endif
extern word_t backend_phys_to_dma_hook( word_t phys );
extern word_t backend_dma_to_phys_hook( word_t dma );
INLINE word_t backend_dma_coherent_check( word_t phys, word_t size )
	{ return true; }
#if defined(CONFIG_GUEST_MODULE_HOOK)
extern bool backend_module_rewrite_hook( elf_ehdr_t *ehdr );
#endif

extern bool backend_request_device_mem( word_t base, word_t size, word_t rwx, bool boot=false);
extern bool backend_request_device_mem_to( word_t base, word_t size, word_t rwx, word_t dest_base, bool boot=false);
extern bool backend_unmap_device_mem( word_t base, word_t size, word_t &rwx, bool boot=false);
extern void backend_unmap_acpi_mem();
extern word_t backend_map_acpi_mem(word_t base);

 
    
// ia32 specific, TODO: relocate
extern void backend_cpuid_override( u32_t func, u32_t max_basic, 
	u32_t max_extended, frame_t *regs );
extern void backend_flush_old_pdir( u32_t new_pdir, u32_t old_pdir );

#include INC_WEDGE(user.h)


extern bool backend_handle_user_pagefault( thread_info_t *thread_info, L4_ThreadId_t tid,  L4_MapItem_t &map_item );
extern void NORETURN 
backend_handle_user_exception( thread_info_t *thread_info );
#if defined(CONFIG_L4KA_VMEXT)
extern void  
backend_handle_user_preemption( thread_info_t *thread_info );
extern void backend_free_pgd_hook( pgent_t *pgdir );
#endif

#if defined(CONFIG_DEVICE_PCI_FORWARD)
extern void backend_pci_config_data_read( pci_config_addr_t addr, u32_t &value, u32_t bit_width, u32_t offset );
extern void backend_pci_config_data_write( pci_config_addr_t addr, u32_t value, u32_t bit_width, u32_t offset );
#endif



#endif /* __AFTERBURN_WEDGE__INCLUDE__L4KA__BACKEND_H__ */
