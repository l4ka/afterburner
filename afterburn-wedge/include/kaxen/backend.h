/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/kaxen/backend.h
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
#ifndef __AFTERBURN_WEDGE__INCLUDE__KAXEN__BACKEND_H__
#define __AFTERBURN_WEDGE__INCLUDE__KAXEN__BACKEND_H__

#include INC_ARCH(cpu.h)
#include INC_ARCH(intlogic.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(memory.h)

#include <burn_counters.h>
#include <elfsimple.h>
#include <aftertime.h>

extern void backend_sync_deliver_vector( word_t vector, bool old_int_state, bool use_error_code, word_t error_code );

//TODO amd64
#ifndef CONFIG_ARCH_AMD64
extern void backend_interruptible_idle( burn_redirect_frame_t *redirect_frame );
#endif
extern void NORETURN backend_activate_user( user_frame_t *user_frame );

INLINE void backend_invalidate_tlb( void )
{
    mmop_queue.tlb_flush(true);
}

// TODO amd64
#ifndef CONFIG_ARCH_AMD64
extern void backend_esp0_sync();
INLINE bool backend_esp0_update()
{
    vcpu_t &vcpu = get_vcpu();
    tss_t *tss = vcpu.cpu.get_tss();

    if( vcpu.xen_esp0 != tss->esp0 )
    {
	INC_BURN_COUNTER(esp0_switch);
	if( XEN_stack_switch(XEN_DS_KERNEL, tss->esp0) )
	    PANIC( "Xen stack switch failure." );
	vcpu.xen_esp0 = tss->esp0;
	xen_do_callbacks();
	return true;
    }

    return false;
}
#endif

INLINE void backend_protect_fpu()
{
    INC_BURN_COUNTER(fpu_protect);
    XEN_fpu_taskswitch();
}

// TODO amd64
#ifndef CONFIG_ARCH_AMD64
INLINE void backend_enable_paging( word_t *ret_address )
{
    xen_memory.enable_guest_paging( get_cpu().cr3.get_pdir_addr() );

    *ret_address += get_vcpu().get_kernel_vaddr();
}
#endif

INLINE void backend_flush_user( word_t pdir_paddr )
{
    // Unused.
}

INLINE void backend_flush_vaddr( word_t vaddr )
{
    xen_memory.flush_vaddr( vaddr );
}

INLINE void backend_write_msr( word_t msr_addr, word_t lower, word_t upper )
{
    dom0_op_t op;
    op.cmd = DOM0_MSR;
    op.interface_version = DOM0_INTERFACE_VERSION;
    op.u.msr.write = 1;
    op.u.msr.cpu_mask = ~0u;
    op.u.msr.msr = msr_addr;
    op.u.msr.in1 = lower;
    op.u.msr.in2 = upper;
    if( XEN_dom0_op(&op) )
	printf( "Xen refused to set an MSR.\n");
}

extern bool backend_enable_device_interrupt( u32_t interrupt, vcpu_t& );
extern bool backend_unmask_device_interrupt( u32_t interrupt );

INLINE void backend_reboot( void )
{
    XEN_shutdown( SHUTDOWN_reboot );
}

INLINE void backend_shutdown( void )
{
    XEN_shutdown( SHUTDOWN_poweroff );
}

extern void backend_exit_hook( void *handle );

extern "C" word_t __attribute__((regparm(1))) backend_pte_read_patch( pgent_t *pgent );
extern "C" word_t __attribute__((regparm(1))) backend_pgd_read_patch( pgent_t *pgent );
extern "C" void __attribute__((regparm(2))) backend_pte_write_patch( pgent_t new_val, pgent_t *old_pgent );
extern "C" void __attribute__((regparm(2))) backend_pte_or_patch( word_t bits, pgent_t *old_pgent );
extern "C" void __attribute__((regparm(2))) backend_pte_and_patch( word_t bits, pgent_t *old_pgent );
extern "C" void __attribute__((regparm(2))) backend_pgd_write_patch( pgent_t new_val, pgent_t *old_pgent );
extern "C" word_t __attribute__((regparm(2))) backend_pte_test_clear_patch( word_t bit, pgent_t *pgent );
extern "C" word_t __attribute__((regparm(2))) backend_pte_xchg_patch( word_t new_val, pgent_t *pgent );

#if defined(CONFIG_GUEST_PTE_HOOK)
extern void backend_set_pte_hook( pgent_t *old_pte, pgent_t new_pte, int level);
extern word_t backend_pte_test_and_clear_hook( pgent_t *pgent, word_t bit );
extern pgent_t backend_pte_get_and_clear_hook( pgent_t *pgent );
extern void backend_free_pgd_hook( pgent_t *pgdir );
#endif

#if defined(CONFIG_GUEST_PTE_HOOK)
extern unsigned long backend_read_pte_hook( pgent_t pgent );
#endif

#if defined(CONFIG_GUEST_DMA_HOOK)
extern word_t backend_dma_coherent_check( word_t phys, word_t size );
extern word_t backend_phys_to_dma_hook( word_t phys, word_t size );
extern word_t backend_dma_to_phys_hook( word_t dma );
#endif

extern void backend_cpuid_override( u32_t func, u32_t max_basic, 
	u32_t max_extended, frame_t *regs );

INLINE void backend_flush_old_pdir( word_t new_pdir, word_t old_pdir )
{
    xen_memory.switch_page_dir( new_pdir, old_pdir );
}

extern pgent_t *backend_resolve_addr( word_t addr, word_t &kernel_vaddr );

#if defined(CONFIG_GUEST_MODULE_HOOK)
extern bool backend_module_rewrite_hook( elf_ehdr_t *ehdr );
#endif

extern time_t backend_get_unix_seconds();

INLINE bool backend_request_device_mem( word_t base, word_t size, word_t rwx, bool boot=false, word_t receive_addr=0)
{   
    ASSERT(size == PAGE_SIZE);
    ASSERT(receive_addr == 0);
    if (boot)
	return xen_memory.map_device_memory(base, base, boot);
    else
    {
	printf("%s UNIMPLEMENTED\n", __FUNCTION__);
	panic();
    }
}    
    
    
INLINE bool backend_unmap_device_mem(word_t base, word_t size, word_t &rwx, bool boot=false)
{ 
    ASSERT(size == PAGE_SIZE);
    if (boot)
	return xen_memory.unmap_device_memory(base, base, boot);
    else
    {
	printf("%s UNIMPLEMENTED\n", __FUNCTION__);
	panic();
    }
}


#endif /* __AFTERBURN_WEDGE__INCLUDE__KAXEN__BACKEND_H__ */
