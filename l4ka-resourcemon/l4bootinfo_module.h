/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     include/l4bootinfo_module.h
 * Description:   Data type for abstracting the L4 boot info records.
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
#ifndef __L4BOOTINFO_MODULE_H__
#define __L4BOOTINFO_MODULE_H__

#include <l4/types.h>
#include <l4/bootinfo.h>

#include <resourcemon.h>
#include <vm_module.h>
#include <basics.h>

class l4bootinfo_module_t : public vm_module_t
{
private:
    void *bootinfo;
    L4_Word_t total;

    void locate_kickstart_turd();
    L4_BootRec_t *get_module( L4_Word_t index );

public:
    bool get_module_info( L4_Word_t index, const char* &cmdline, 
	    L4_Word_t &haddr_start, L4_Word_t &size );
    
    void *get_bootinfo() { return bootinfo; }

    L4_Word_t get_total();
    bool init();

    l4bootinfo_module_t()
	{ bootinfo = NULL; total = 0; }
};

#endif	/* __L4BOOTINFO_MODULE_H__ */

