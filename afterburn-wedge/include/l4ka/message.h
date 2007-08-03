/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4-common/message.h
 * Description:   Wrappers for building and extracting L4 messages.
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
 * $Id: message.h,v 1.6 2005/04/13 15:47:32 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4_COMMON__MESSAGE_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4_COMMON__MESSAGE_H__

enum msg_label_e {
    msg_label_virq = 0x100,
    msg_label_ipi = 0x101,
    msg_label_ipi_done = 0x102,
    msg_label_vector = 0x103,
    msg_label_device_enable = 0x104,
    msg_label_device_disable = 0x105,
    msg_label_device_done = 0x106,
    msg_label_ts_donation = 0x107,
    msg_label_thread_create = 0x108,
    msg_label_thread_create_done = 0x109,
    msg_label_exception = 0xffb0,
    msg_label_preemption = 0xffd0,
    msg_label_preemption_yield = 0xffd1,
    msg_label_preemption_reply = 0xffd2, 
    msg_label_startup_reply = 0xffd3, 
    msg_label_pfault_start = 0xffe0, 
    msg_label_pfault_end = 0xffe7, 
    msg_label_hwirq = 0xfff0, 
    msg_label_hwirq_ack = 0xfff1,

};

INLINE bool msg_is_virq( L4_MsgTag_t tag )
{
    return (L4_Label(tag) == msg_label_virq) && (L4_UntypedWords(tag) >= 1);
}


INLINE void msg_virq_extract( L4_Word_t *irq )
{
    L4_StoreMR( 1, irq );
}

INLINE void msg_virq_build( L4_Word_t irq )
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.u = 1;
    tag.X.label = msg_label_virq;

    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, irq );
}

INLINE void msg_ipi_extract( L4_Word_t *src_vcpu_id, L4_Word_t *vector )
{
    L4_StoreMR( 1, src_vcpu_id );
    L4_StoreMR( 2, vector );
}

INLINE void msg_ipi_build( L4_Word_t src_vcpu_id, L4_Word_t vector )
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.u = 2;
    tag.X.label = msg_label_ipi;
    
    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, src_vcpu_id );
    L4_LoadMR( 2, vector );
}

INLINE void msg_ipi_done_build( )
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.label = msg_label_ipi_done;
    L4_Set_MsgTag( tag );
}

INLINE bool msg_is_hwirq_ack( L4_MsgTag_t tag )
{
    return (L4_Label(tag) == msg_label_hwirq_ack) 
	&& (L4_UntypedWords(tag) >= 1);
}


INLINE void msg_hwirq_ack_extract( L4_Word_t *irq )
{
    L4_StoreMR( 1, irq );
}

INLINE void msg_hwirq_ack_build( L4_Word_t irq, L4_ThreadId_t virtualsender = L4_nilthread )
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.u = 1;
    tag.X.label = msg_label_hwirq_ack;
    if (virtualsender != L4_nilthread)
    {
	L4_Set_Propagation(&tag);
	L4_Set_VirtualSender(virtualsender);
    }
    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, irq );
}

INLINE bool msg_is_device_enable( L4_MsgTag_t tag )
{
    return (L4_Label(tag) == msg_label_device_enable)
	&& (L4_UntypedWords(tag) >= 1);
}


INLINE void msg_device_enable_extract( L4_Word_t *irq )
{
    L4_StoreMR( 1, irq );
}

INLINE void msg_device_enable_build( L4_Word_t irq )
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.u = 1;
    tag.X.label = msg_label_device_enable;

    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, irq );
}

INLINE void msg_device_disable_extract( L4_Word_t *irq )
{
    L4_StoreMR( 1, irq );
}

INLINE void msg_device_disable_build( L4_Word_t irq )
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.u = 1;
    tag.X.label = msg_label_device_disable;
    
    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, irq );
}

INLINE void msg_device_done_build( )
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.label = msg_label_device_done;
    L4_Set_MsgTag( tag );
}


INLINE bool msg_is_vector( L4_MsgTag_t tag )
{
    return (L4_Label(tag) == msg_label_vector) && 
	(L4_UntypedWords(tag) >= 2);
}

INLINE void msg_vector_extract( L4_Word_t *vector, L4_Word_t *irq )
{
    L4_StoreMR( 1, vector );
    L4_StoreMR( 2, irq );
}

INLINE void msg_vector_build( L4_Word_t vector, L4_Word_t irq )
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.u = 2;
    tag.X.label = msg_label_vector;

    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, vector );
    L4_LoadMR( 2, irq );
}


INLINE void msg_ts_donation_build(L4_ThreadId_t src)
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.u = 1;
    tag.X.label = msg_label_ts_donation;
    L4_LoadMR( 1, src.raw);

    L4_Set_MsgTag( tag );
}

INLINE void msg_ts_donation_extract(L4_ThreadId_t *src)
	
{
    L4_StoreMR( 1, &src->raw );
}

INLINE void msg_thread_create_build(void *vcpu,
	L4_Word_t stack_bottom,
	L4_Word_t stack_size,
	L4_Word_t prio,
	void *start_func,
	L4_ThreadId_t pager_tid,
	void *start_param,
	void *tlocal_data,
	L4_Word_t tlocal_size)
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.u = 9;
    tag.X.label = msg_label_thread_create; 
    
    L4_Set_MsgTag( tag );
    L4_LoadMR(1, (L4_Word_t) vcpu);
    L4_LoadMR(2, (L4_Word_t) stack_bottom);
    L4_LoadMR(3, (L4_Word_t) stack_size);
    L4_LoadMR(4, (L4_Word_t) prio);
    L4_LoadMR(5, (L4_Word_t) start_func);
    L4_LoadMR(6, (L4_Word_t) pager_tid.raw);
    L4_LoadMR(7, (L4_Word_t) start_param);
    L4_LoadMR(8, (L4_Word_t) tlocal_data);
    L4_LoadMR(9, (L4_Word_t) tlocal_size);
    
}

INLINE void msg_thread_create_extract(void **vcpu,
	L4_Word_t *stack_bottom,
	L4_Word_t *stack_size,
	L4_Word_t *prio,
	void *start_func,
	L4_ThreadId_t *pager_tid,
	void **start_param,
	void **tlocal_data,
	L4_Word_t *tlocal_size)
	
{
    L4_StoreMR(1, (L4_Word_t *) vcpu);
    L4_StoreMR(2, (L4_Word_t *) stack_bottom);
    L4_StoreMR(3, (L4_Word_t *) stack_size);
    L4_StoreMR(4, (L4_Word_t *) prio);
    L4_StoreMR(5, (L4_Word_t *) start_func);
    L4_StoreMR(6, (L4_Word_t *) pager_tid);
    L4_StoreMR(7, (L4_Word_t *) start_param);
    L4_StoreMR(8, (L4_Word_t *) tlocal_data);
    L4_StoreMR(9, (L4_Word_t *) tlocal_size);
   
}

INLINE void msg_thread_create_done_build(void *hthread)
{
    L4_MsgTag_t tag = L4_Niltag;
    tag.X.label = msg_label_thread_create_done;
    tag.X.u = 1;
    L4_Set_MsgTag( tag );
    L4_LoadMR(1, (L4_Word_t) hthread);

}

INLINE void msg_thread_create_done_extract(void **hthread)	
{
    L4_StoreMR(1, (L4_Word_t *) hthread);
}

#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4_COMMON__MESSAGE_H__ */
