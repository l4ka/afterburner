/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     l4ka/backend.cc
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

#include <l4/kip.h>
#include INC_WEDGE(message.h)
#include INC_WEDGE(vt/vm.h)

static const bool debug_pdir_flush=0;
static const bool debug_global_page=0;
static const bool debug_unmask_device_irq=0;

void backend_flush_user( void )
{
    UNIMPLEMENTED();
}

INLINE L4_ThreadId_t interrupt_to_tid( u32_t interrupt )
{
    L4_ThreadId_t tid;
    tid.global.X.thread_no = interrupt;
    tid.global.X.version = 1;
    return tid;
}

bool backend_enable_device_interrupt( u32_t interrupt )
{
    con << "enable device " << interrupt << "\n";
    //L4_KDB_Enter("enable_device");
    
    msg_device_enable_build( interrupt );
    L4_MsgTag_t tag = L4_Send( get_vm()->get_vcpu().irq_ltid );
    if( L4_IpcFailed(tag) ) {
	con << L4_ErrorCode() << "\n";
    }

    return !L4_IpcFailed(tag);
}

bool backend_unmask_device_interrupt( u32_t interrupt )
{
#if 0
    msg_device_ack_build( interrupt );
    L4_MsgTag_t tag = L4_Send( get_vcpu().irq_ltid );
    ASSERT( !L4_IpcFailed(tag) );
#endif

    L4_ThreadId_t ack_tid = L4_nilthread;
    L4_MsgTag_t tag = { raw : 0 };
    L4_Set_Propagation (&tag);
    L4_Set_VirtualSender(get_vm()->get_vcpu().irq_gtid);
    
    ack_tid.global.X.thread_no = interrupt;
    ack_tid.global.X.version = 1;

    L4_LoadMR( 0, tag.raw );  // Ack msg.
    
    if (debug_unmask_device_irq)
	con << "Unmask IRQ " << interrupt << " via propagation\n";
    
    //msg_device_ack_build( interrupt );
    tag = L4_Reply( ack_tid );
    ASSERT( !L4_IpcFailed(tag) );
    return !L4_IpcFailed(tag);
}

u32_t backend_get_nr_device_interrupts()
{
    return  L4_ThreadIdSystemBase(L4_GetKernelInterface());
}

void backend_reboot( void )
{
    L4_KDB_Enter("VM reboot");
    /* Return to the caller and take a chance at completing a hardware
     * reboot.
     */
}

bool backend_sync_deliver_vector( L4_Word_t vector, bool old_int_state, bool use_error_code, L4_Word_t error_code )
{


    return false;
}

