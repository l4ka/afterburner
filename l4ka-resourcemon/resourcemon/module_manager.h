/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/include/module_manager.h
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
#ifndef __HYPERVISOR__INCLUDE__MODULE_MANAGER_H__
#define __HYPERVISOR__INCLUDE__MODULE_MANAGER_H__

#include <l4/types.h>

#include <common/mbi_module.h>
#include <common/l4bootinfo_module.h>
#include <common/elf.h>
#include <common/string.h>

class module_manager_t
{
private:
    mbi_module_t mbi;
    l4bootinfo_module_t l4bootinfo;
    vm_module_t *vm_modules;

    L4_Word_t current_module;
    L4_Word_t current_ramdisk_module;
    bool ramdisk_valid;
    L4_Word_t module_clones;

    
    const char *cmdline_options( const char *cmdline );
    bool cmdline_has_substring( const char *cmdline, const char *substr, char **start = NULL);
    L4_Word_t parse_size_option( const char *name, const char *option );

    L4_Word_t get_module_param_size( const char *token, const char *cmdline );
    L4_Word_t get_module_memsize( const char *cmdline );
    
    void init_dhcp_info();
    
    bool cmdline_has_ramdisk( const char *cmdline )
    {
	return cmdline_has_substring(cmdline, "/dev/ram") ||
	    cmdline_has_substring(cmdline, "/dev/rd/") ||
	    cmdline_has_substring(cmdline, "initrd");
    }
    
    bool cmdline_has_vmstart( const char *cmdline )
	{ return cmdline_has_substring(cmdline, "vmstart"); }

    bool cmdline_has_ddos( const char *cmdline )
	{ return cmdline_has_substring(cmdline, "ddos"); }

    bool cmdline_disables_client_dma( const char *cmdline )
	{ return cmdline_has_substring(cmdline, "noclientdma"); }

    bool cmdline_has_tracing( const char *cmdline )
	{ return cmdline_has_substring(cmdline, "trace"); }


public:
    bool init();
    void dump_modules_list();

    bool load_current_module();
    bool next_module();

    bool is_valid_binary( L4_Word_t binary_start )
	{ return elf_is_valid(binary_start); }

    module_manager_t() 
    { 
	current_module = current_ramdisk_module;
	module_clones = 1;
	ramdisk_valid = false;
	vm_modules = NULL;
    }
    
    bool cmdline_has_grubdhcp( const char *cmdline, char **start, char **end )
	{ 
	    static const char *substr = "ip=grubdhcp";
	    bool ret =  cmdline_has_substring(cmdline, substr, start);
	    *end = *start + strlen(substr);	
	    return ret;
	}

    struct dhcp_info_t
    {	
	char ip[15], mask[15], server[15], gateway[15];
    } dhcp_info;
    
};

extern inline module_manager_t * get_module_manager()
{
    extern module_manager_t module_manager;
    return &module_manager;
}

#endif	/* __HYPERVISOR__INCLUDE__MODULE_MANAGER_H__ */
