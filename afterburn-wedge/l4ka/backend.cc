/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     l4-common/backend.cc
 * Description:   Common L4 arch-independent backend routines.
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

#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(l4privileged.h)
#include <debug.h>
#include INC_WEDGE(irq.h)
#include INC_WEDGE(message.h)

#include <l4/kip.h>


L4_MsgTag_t backend_notify_thread( L4_ThreadId_t tid, L4_Time_t timeout)
{
#if defined(CONFIG_L4KA_VMEXT)
    if (L4_Myself() == get_vcpu().main_gtid)
	get_vcpu().main_info.mr_save.load_yield_msg(L4_nilthread);
    return L4_Ipc( tid, L4_anythread,  L4_Timeouts(timeout,L4_Never), &tid);
#else
    return L4_Send( tid, timeout );
#endif
}

void backend_flush_user( word_t pdir_paddr )
{
    vcpu_t &vcpu = get_vcpu();

#if defined(CONFIG_L4KA_VT)
    UNIMPLEMENTED();
#endif
    
#if defined(CONFIG_L4KA_VMEXT)
    L4_ThreadId_t tid;
    task_info_t *ti;
    thread_info_t *thi;

    if ((ti = get_task_manager().find_by_page_dir(pdir_paddr, false)) &&
	(thi = ti->get_vcpu_thread(vcpu.cpu_id, false)))
	tid = thi->get_tid();
    else
	tid = L4_nilthread;
	
    dprintf(debug_task, "flush user pdir %wx tid %t\n", 
	    pdir_paddr, tid);
#endif
    
#if 0
    L4_Flush( L4_CompleteAddressSpace + L4_FullyAccessible );
#else
    // Pistachio is currently broken, and unmaps the kip and utcb.
    if( vcpu.get_kernel_vaddr() == GB(2UL) )
    	L4_Flush( L4_FpageLog2(0, 31) + L4_FullyAccessible );
    else if( vcpu.get_kernel_vaddr() == GB(1UL) )
    	L4_Flush( L4_FpageLog2(0, 30) + L4_FullyAccessible );
    else if( vcpu.get_kernel_vaddr() == MB(1536UL) ) {
    	L4_Flush( L4_FpageLog2(0, 30) + L4_FullyAccessible );
    	L4_Flush( L4_FpageLog2(GB(1), 29) + L4_FullyAccessible );
    }
    else if( vcpu.get_kernel_vaddr() == MB(256UL) )
	L4_Flush( L4_Fpage(0, MB(256)) + L4_FullyAccessible );
    else if( vcpu.get_kernel_vaddr() == MB(8UL) )
    	L4_Flush( L4_FpageLog2(0, 23) + L4_FullyAccessible );
    else if( vcpu.get_kernel_vaddr() == 0 ) {
	printf( "Skipping address space flush, and assume that user-apps\n"
		"aren't ever mapped into the kernel's address space.\n");
    }
    else
	PANIC( "Unsupported kernel link address: %x", vcpu.get_kernel_vaddr());
#endif
}

INLINE L4_ThreadId_t interrupt_to_tid( u32_t interrupt )
{
    L4_ThreadId_t tid;
    tid.global.X.thread_no = interrupt;
    tid.global.X.version = 1;
    return tid;
}

bool backend_enable_device_interrupt( u32_t interrupt, vcpu_t &vcpu )
{
    intlogic_t &intlogic = get_intlogic();
    ASSERT( !get_vcpu().cpu.interrupts_enabled() );
    
    if (intlogic.is_hwirq_squashed(interrupt) || 
	intlogic.is_virtual_hwirq(interrupt))
	return true;

    msg_device_enable_build( interrupt );
    L4_MsgTag_t tag = L4_Call( vcpu.irq_gtid );
    ASSERT( !L4_IpcFailed(tag) );
    return !L4_IpcFailed(tag);
}


bool backend_disable_device_interrupt( u32_t interrupt, vcpu_t &vcpu )
{
    intlogic_t &intlogic = get_intlogic();
    ASSERT( !get_cpu().interrupts_enabled() );
    
    if (intlogic.is_hwirq_squashed(interrupt) || 
	intlogic.is_virtual_hwirq(interrupt))
	return true;
    
    msg_device_disable_build( interrupt );
    L4_MsgTag_t tag = L4_Call( vcpu.irq_gtid );
    ASSERT( !L4_IpcFailed(tag) );
    return !L4_IpcFailed(tag);
}


bool backend_unmask_device_interrupt( u32_t interrupt )
{
   
    ASSERT( !get_vcpu().cpu.interrupts_enabled() );
    L4_ThreadId_t ack_tid;
    L4_MsgTag_t tag = L4_Niltag;
    intlogic_t &intlogic = get_intlogic();
    
    if (intlogic.is_virtual_hwirq(interrupt))
    {	
	ack_tid = intlogic.get_virtual_hwirq_sender(interrupt);	
	ASSERT(ack_tid != L4_nilthread);
	printf("Unmask virtual IRQ sender %t %d\n", ack_tid, interrupt);
	dprintf(irq_dbg_level(interrupt), "Unmask virtual IRQ sender %t %d\n", ack_tid, interrupt);
    }
    else 
    {
	if (intlogic.is_hwirq_squashed(interrupt))
	    return true;

	dprintf(irq_dbg_level(interrupt), "Unmask IRQ %d\n", interrupt);
	ack_tid = get_vcpu().get_hwirq_tid(interrupt);
    }
    
    msg_hwirq_ack_build( interrupt, get_vcpu().irq_gtid);
    tag = backend_notify_thread(ack_tid, L4_Never);
    
    if (L4_IpcFailed(tag))
    {
	printf( "Unmask IRQ %d via propagation failed L4 error: %d\n", 
		interrupt, L4_ErrorCode());
	DEBUGGER_ENTER("BUG");
    }
	    
    return L4_IpcFailed(tag);
}



u32_t backend_get_nr_device_interrupts()
{
    L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
    return  L4_ThreadIdSystemBase(kip);
}

void backend_reboot( void )
{
    DEBUGGER_ENTER("VM reboot");
    /* Return to the caller and take a chance at completing a hardware
     * reboot.
     */
}

