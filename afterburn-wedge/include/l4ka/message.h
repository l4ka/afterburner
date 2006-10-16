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
    msg_label_vector = 0x102,
    msg_label_device_ack = 0x103,
    msg_label_device_enable = 0x104,
    msg_label_device_disable = 0x105,
    msg_label_exception = 0xffb0,
    msg_label_pfault_start = 0xffe0, 
    msg_label_pfault_end = 0xffe7, 
    
};

INLINE bool msg_is_device_ack( L4_MsgTag_t tag )
{
    return (L4_Label(tag) == msg_label_device_ack) 
	&& (L4_UntypedWords(tag) >= 1);
}

INLINE void msg_device_ack_extract( L4_Word_t *irq )
{
    L4_StoreMR( 1, irq );
}

INLINE void msg_device_ack_build( L4_Word_t irq )
{
    L4_MsgTag_t tag;
    tag.raw = 0;
    tag.X.u = 1;
    tag.X.label = msg_label_device_ack;

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
    L4_MsgTag_t tag;
    tag.raw = 0;
    tag.X.u = 1;
    tag.X.label = msg_label_device_enable;

    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, irq );
}

INLINE bool msg_is_device_disable( L4_MsgTag_t tag )
{
    return (L4_Label(tag) == msg_label_device_disable)
	&& (L4_UntypedWords(tag) >= 1);
}

INLINE void msg_device_disable_extract( L4_Word_t *irq )
{
    L4_StoreMR( 1, irq );
}

INLINE void msg_device_disable_build( L4_Word_t irq )
{
    L4_MsgTag_t tag;
    tag.raw = 0;
    tag.X.u = 1;
    tag.X.label = msg_label_device_disable;
    
    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, irq );
}

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
    L4_MsgTag_t tag;
    tag.raw = 0;
    tag.X.u = 1;
    tag.X.label = msg_label_virq;

    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, irq );
}

INLINE bool msg_is_vector( L4_MsgTag_t tag )
{
    return (L4_Label(tag) == msg_label_vector) && 
	(L4_UntypedWords(tag) >= 1);
}

INLINE void msg_vector_extract( L4_Word_t *vector )
{
    L4_StoreMR( 1, vector );
}

INLINE void msg_vector_build( L4_Word_t vector )
{
    L4_MsgTag_t tag;
    tag.raw = 0;
    tag.X.u = 1;
    tag.X.label = msg_label_vector;

    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, vector );
}

INLINE bool msg_is_ipi( L4_MsgTag_t tag )
{
    return (L4_Label(tag) == msg_label_ipi) && (L4_UntypedWords(tag) >= 1);
}


INLINE void msg_ipi_extract( L4_Word_t *src_vcpu_id, L4_Word_t *vector )
{
    L4_StoreMR( 1, src_vcpu_id );
    L4_StoreMR( 2, vector );
}

INLINE void msg_ipi_build( L4_Word_t src_vcpu_id, L4_Word_t vector )
{
    L4_MsgTag_t tag;
    tag.raw = 0;
    tag.X.u = 2;
    tag.X.label = msg_label_ipi;
    
    L4_Set_MsgTag( tag );
    L4_LoadMR( 1, src_vcpu_id );
    L4_LoadMR( 2, vector );
}


INLINE void msg_startup_build( L4_Word_t ip, L4_Word_t sp )
{
    L4_Msg_t msg;
    L4_Clear( &msg );
    L4_Append( &msg, ip );
    L4_Append( &msg, sp );
    L4_Load( &msg );
}

#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4_COMMON__MESSAGE_H__ */
