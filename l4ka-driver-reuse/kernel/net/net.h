/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/net/net.h
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
#ifndef __L4KA_DRIVERS__NET__NET_H__
#define __L4KA_DRIVERS__NET__NET_H__

#include <l4/types.h>
#include <l4/kdebug.h>

#include <linux/config.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/sock.h>
#include <net/pkt_sched.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>

#include <glue/vmserver.h>
#include <glue/vmirq.h>
#include <glue/vmmemory.h>

#if IDL4_HEADER_REVISION < 20030403
# error "Your version of IDL4 is too old.  Please upgrade to the latest."
#endif

#if defined(TRUE)
#undef TRUE
#endif
#define TRUE	(1 == 1)

#if defined(FALSE)
#undef FALSE
#endif

#define FALSE	(1 == 0)

#define RAW(a)	((void *)((a).raw))

#if defined(AFTERBURN_DRIVERS_NET_OPTIMIZE)

#define L4VMnet_debug_level	2

#define PARANOID(a)		// Don't execute paranoid stuff.
#define ASSERT(a)		// Don't execute asserts.

#else

extern int L4VMnet_debug_level;

#define PARANOID(a)		a
#define ASSERT(a)		do { if(!(a)) { printk( KERN_CRIT PREFIX "assert failure %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__); L4_KDB_Enter("assert"); } } while(0)

#endif	/* AFTERBURN_DRIVERS_NET_OPTIMIZE */

#define dprintk(n,a...) do { if(L4VMnet_debug_level >= (n)) printk(a); } while(0)


#define L4VMNET_SKB_RING_LEN	256

#define L4_TAG_IRQ	0x100

typedef struct L4VMnet_skb_ring
{
    struct sk_buff *skb_ring[L4VMNET_SKB_RING_LEN];
    L4_Word16_t cnt;
    L4_Word16_t start_free;
    L4_Word16_t start_dirty;
} L4VMnet_skb_ring_t;

extern inline L4_Word16_t L4VMnet_skb_ring_pending( L4VMnet_skb_ring_t *ring )
{
    return (ring->start_free + ring->cnt - ring->start_dirty) % ring->cnt;
}

typedef struct
{
    struct sk_buff *skb;
} L4VMnet_shadow_t;

typedef struct
{
    IVMnet_ring_descriptor_t *desc_ring;
    L4_Word16_t cnt;
    L4_Word16_t start_free;
    L4_Word16_t start_dirty;
} L4VMnet_desc_ring_t;

extern inline L4_Word16_t
L4VMnet_ring_available( L4VMnet_desc_ring_t *ring )
{
    return (ring->start_dirty + ring->cnt - (ring->start_free + 1)) % ring->cnt;
}

extern inline L4_Word_t
L4VMnet_fragment_buffer( struct skb_frag_struct *frag )
{
    return (L4_Word_t)(frag->page - mem_map) * PAGE_SIZE + frag->page_offset;
}

extern int L4VMnet_allocate_lan_address( char lanaddress[ETH_ALEN] );
extern void L4VMnet_print_lan_address( unsigned char *lanaddr );

#endif	/* __L4KA_DRIVERS__NET__NET_H__ */
