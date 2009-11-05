/*********************************************************************
 *                
 * Copyright (C) 2004, 2008-2009 Joshua LeVasseur
 *
 * File path:	block/block.h
 * Description:	Common declarations for the server and client of the
 * 		Linux block driver.
 *
 * Proprietary!  DO NOT DISTRIBUTE!
 *
 * $Id: block.h,v 1.1 2006/09/21 09:28:35 joshua Exp $
 *                
 ********************************************************************/
#ifndef __linuxblock__L4VMblock_h__
#define __linuxblock__L4VMblock_h__

#include <linux/version.h>
#include <l4/kdebug.h>
#include <glue/wedge.h>

//#define CONFIG_AFTERBURN_DRIVERS_BLOCK_OPTIMIZE

/*
#if IDL4_HEADER_REVISION < 20031207
# error "Your version of IDL4 is too old.  Please upgrade to the latest."
#endif
*/

#undef TRUE
#undef FALSE

#define TRUE	1
#define FALSE	0


#define RAW(a)	((void *)((a).raw))

#if defined(CONFIG_AFTERBURN_DRIVERS_BLOCK_OPTIMIZE)
#define PARANOID(a)		// Don't execute paranoid stuff.
#define ASSERT(a)		// Don't execute asserts.
#define dprintk(n,a...)		do {} while(0)

#else

#define PARANOID(a)		a
#define ASSERT(a)		do { if(!(a)) { printk( PREFIX "assert failure %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__); L4_KDB_Enter("assert"); } } while(0)

extern int L4VMblock_debug_level;
#define L4VMblock_debug_id	60
#define dprintk(n,a...) do { l4ka_wedge_debug_printf(L4VMblock_debug_id, L4VMblock_debug_level + n, a); } while(0)

#endif	/* CONFIG_AFTERBURN_DRIVERS_BLOCK_OPTIMIZE */


typedef struct 
{
    L4_Word16_t cnt;
    L4_Word16_t start_free;
    L4_Word16_t start_dirty;
} L4VMblock_ring_t;

extern inline L4_Word16_t
L4VMblock_ring_available( L4VMblock_ring_t *ring )
{
    return (ring->start_dirty + ring->cnt - (ring->start_free + 1)) % ring->cnt;
}

#define L4VMBLOCK_SERVER_IRQ_DISPATCH	(1)
#define L4VMBLOCK_CLIENT_IRQ_CLEAN	(1)

#endif	/* __linuxblock__L4VMblock_h__ */
