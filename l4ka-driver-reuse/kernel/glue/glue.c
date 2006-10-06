/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:	l4ka-drivers/glue/glue.c
 * Description:	Module declarations.
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

#include <linux/kernel.h>
#include <linux/module.h>

#include "wedge.h"


MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Joshua LeVasseur <jtl@ira.uka.de>");
MODULE_DESCRIPTION("Generic support for modules that wish to interact with the L4Ka wedge.");
MODULE_VERSION("yoda");

#if defined(MODULE)
// Define a .afterburn_wedge_module section, to activate sybmol resolution for
// this module against the wedge's symbols.
__attribute__ ((section(".afterburn_wedge_module"))) 
burn_wedge_header_t burn_wedge_header = { 
    BURN_WEDGE_MODULE_INTERFACE_VERSION
};
#endif

static int __init glue_init( void )
{
    printk( KERN_INFO "L4Ka glue module.\n" );

    printk( KERN_INFO "Wedge configuration:\n" );
    printk( KERN_INFO "  L4 priority : %lu\n", resourcemon_shared.prio );
    printk( KERN_INFO "  TID space start: 0x%p\n", 
	    (void *)resourcemon_shared.thread_space_start );
    printk( KERN_INFO "  TID space len  : %lu\n", 
	    resourcemon_shared.thread_space_len );
    printk( KERN_INFO "  UTCB at 0x%p, size %lu\n",
	    (void *)L4_Address(resourcemon_shared.utcb_fpage),
	    L4_Size(resourcemon_shared.utcb_fpage) );
    printk( KERN_INFO "  KIP at 0x%p, size %lu\n",
	    (void *)L4_Address(resourcemon_shared.kip_fpage),
	    L4_Size(resourcemon_shared.kip_fpage) );
    printk( KERN_INFO "  Allocated memory: 0x%p\n", 
	    (void *)resourcemon_shared.phys_size );
    printk( KERN_INFO "  End of physical memory: 0x%p\n", 
	    (void *)resourcemon_shared.phys_end );
    printk( KERN_INFO "  Main TID    : 0x%p\n",
	    (void *)get_vcpu()->main_gtid.raw );
    printk( KERN_INFO "  IRQ TID     : 0x%p\n",
	    (void *)get_vcpu()->irq_gtid.raw );
    printk( KERN_INFO "  Monitor TID : 0x%p\n",
	    (void *)get_vcpu()->monitor_gtid.raw);

    l4ka_wedge_declare_pdir_master( init_mm.pgd );

    return 0;
}

static void __exit glue_exit( void )
{
}

module_init( glue_init );
module_exit( glue_exit );

