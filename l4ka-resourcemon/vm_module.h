/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     include/vm_module.h
 * Description:   Abstract data type for representing a boot loader module.
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
#ifndef __VM_MODULE_H__
#define __VM_MODULE_H__

#include <l4/types.h>
#include <l4/kip.h>

typedef enum {
    bootloader_memdesc_undefined=   (0<<4) + L4_BootLoaderSpecificMemoryType,
    bootloader_memdesc_init_table=  (1<<4) + L4_BootLoaderSpecificMemoryType,
    bootloader_memdesc_init_server= (2<<4) + L4_BootLoaderSpecificMemoryType,
    bootloader_memdesc_boot_module= (3<<4) + L4_BootLoaderSpecificMemoryType,
} bootloader_memdesc_e;


class vm_module_t
{
public:
    virtual bool get_module_info( L4_Word_t index, const char* &cmdline, 
	    L4_Word_t &haddr_start, L4_Word_t &size ) = 0;

    virtual L4_Word_t get_total() = 0;
    virtual bool init() = 0;
};


#endif	/* __VM_MODULE_H__ */
