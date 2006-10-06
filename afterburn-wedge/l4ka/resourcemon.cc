/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/resourcemon.cc
 * Description:   Functions for interacting with the resource monitor.
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

#include INC_ARCH(cpu.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(resourcemon.h)

#include <memory.h>

#if defined(CONFIG_DEVICE_DP83820) || defined(CONFIG_DEVICE_PCI_FORWARD)

INLINE L4_ThreadId_t l4ka_locator_tid()
{
    return resourcemon_shared.cpu[L4_ProcessorNo()].locator_tid;
}


bool l4ka_server_locate( guid_t guid, L4_ThreadId_t *server_tid )
{
    cpu_t &cpu = get_cpu();
    CORBA_Environment ipc_env = idl4_default_environment;

    bool irq_save = cpu.disable_interrupts();
    IResourcemon_query_interface( l4ka_locator_tid(), 
	    guid, server_tid, &ipc_env );
    cpu.restore_interrupts( irq_save );
 
    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	return false;
    }
    return true;
}

#endif

bool cmdline_key_search( const char *key, char *value, word_t n )
{
    const char *found = strstr( resourcemon_shared.cmdline, key );
    if( !found )
	return false;

    found += strlen(key);

    // Copy the parameter into 'value'.
    n--;  // Leave space for the terminator.
    word_t i = 0;
    while( (i < n) && (*found != '\0') && (*found != '\n') 
	    && (*found != '\t') && (*found != ' ') )
	value[i++] = *found++;
    value[i] = '\0';

    return true;
}

