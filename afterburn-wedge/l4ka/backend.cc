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

#include INC_WEDGE(console.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(setup.h)
#include INC_WEDGE(irq.h)

#include <l4/kip.h>
#include INC_WEDGE(message.h)

static const bool debug_pdir_flush=0;
static const bool debug_global_page=0;

void backend_flush_user( void )
{
    vcpu_t &vcpu = get_vcpu();

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
	con << "Skipping address space flush, and assume that user-apps\n"
	       "aren't ever mapped into the kernel's address space.\n";
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
#if 0
    L4_ThreadId_t irq_tid = interrupt_to_tid( interrupt );
    L4_ThreadId_t handler_tid = get_vcpu().irq_gtid;

    return AssociateInterrupt( irq_tid, handler_tid );
#else
    ASSERT( !get_vcpu().cpu.interrupts_enabled() );
    msg_device_enable_build( interrupt );
    L4_MsgTag_t tag = L4_Call( vcpu.irq_gtid );
    ASSERT( !L4_IpcFailed(tag) );
    return !L4_IpcFailed(tag);
#endif
}


bool backend_disable_device_interrupt( u32_t interrupt, vcpu_t &vcpu )
{
    ASSERT( !get_cpu().interrupts_enabled() );
    msg_device_disable_build( interrupt );
    L4_MsgTag_t tag = L4_Call( vcpu.irq_gtid );
    ASSERT( !L4_IpcFailed(tag) );
    return !L4_IpcFailed(tag);
}


bool backend_unmask_device_interrupt( u32_t interrupt )
{
    ASSERT( !get_vcpu().cpu.interrupts_enabled() );
    L4_MsgTag_t tag = L4_Niltag;
    
    if (debug_hwirq || 
	    get_intlogic().is_irq_traced(interrupt))
	con << "Unmask IRQ " << interrupt << " via propagation\n";
	
#if defined(CONFIG_L4KA_VMEXTENSIONS)
    L4_ThreadId_t ack_tid = virq_tid;
    msg_hwirq_ack_build( interrupt, get_vcpu().irq_gtid);
    tag = L4_Call( ack_tid );
#else
    L4_ThreadId_t ack_tid = L4_nilthread;
    ack_tid.global.X.thread_no = interrupt;
    ack_tid.global.X.version = 1;
    msg_hwirq_ack_build( interrupt, get_vcpu().irq_gtid);
    tag = L4_Call( ack_tid );
#endif
    
    if (L4_IpcFailed(tag))
    {
	con << "Unmask IRQ " << interrupt << " via propagation failed "
	    << "ErrCore " << L4_ErrorCode() << "\n";
    }
    return L4_IpcFailed(tag);
}



u32_t backend_get_nr_device_interrupts()
{
    ASSERT(kip);
    return  L4_ThreadIdSystemBase(kip);
}

void backend_reboot( void )
{
    L4_KDB_Enter("VM reboot");
    /* Return to the caller and take a chance at completing a hardware
     * reboot.
     */
}

