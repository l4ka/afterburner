/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/pci_forward.cc
 * Description:   Forwards PCI accesses to a PCI driver, via L4 IPC.
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

#include <device/pci.h>

#include "L4VMpci_idl_client.h"

static L4_ThreadId_t pci_server_tid = L4_nilthread;
static bool pci_server_disabled = false;

static bool pci_server_attach()
{
    IVMpci_dev_id_t devs[1] = 
	{ {CONFIG_PCI_FORWARD_VENDOR, CONFIG_PCI_FORWARD_DEVICE} };

    if( !l4ka_server_locate(UUID_IVMpci, &pci_server_tid) ) {
	pci_server_tid = L4_nilthread;
	pci_server_disabled = true;
	return false;
    }

    cpu_t &cpu = get_cpu();
    CORBA_Environment ipc_env = idl4_default_environment;
    bool irq_save = cpu.disable_interrupts();
    IVMpci_Control_register( pci_server_tid, 1, devs, &ipc_env );
    cpu.restore_interrupts( irq_save );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	L4_KDB_Enter( "PCI forward attach failure" );
	pci_server_tid = L4_nilthread;
	pci_server_disabled = true;
    }

    return true;
}

void backend_pci_config_data_read( pci_config_addr_t addr, u32_t &value, u32_t bit_width, u32_t bit_offset )
{
    if( pci_server_disabled )
	return;
    if( L4_IsNilThread(pci_server_tid) && !pci_server_attach() )
    	return;

    cpu_t &cpu = get_cpu();
    CORBA_Environment ipc_env = idl4_default_environment;
    bool irq_save = cpu.disable_interrupts();
    IVMpci_Control_read( pci_server_tid, 0, addr.x.fields.reg,
	    bit_width/8, bit_offset/8, &value, &ipc_env);
    cpu.restore_interrupts( irq_save );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	L4_KDB_Enter( "PCI forward failure" );
    }
}

void backend_pci_config_data_write( pci_config_addr_t addr, u32_t value, u32_t bit_width, u32_t bit_offset )
{
    if( pci_server_disabled )
	return;
    if( L4_IsNilThread(pci_server_tid) && !pci_server_attach() )
    	return;

    cpu_t &cpu = get_cpu();
    CORBA_Environment ipc_env = idl4_default_environment;
    bool irq_save = cpu.disable_interrupts();
    IVMpci_Control_write( pci_server_tid, 0, addr.x.fields.reg,
	    bit_width/8, bit_offset/8, value, &ipc_env);
    cpu.restore_interrupts( irq_save );

    if( ipc_env._major != CORBA_NO_EXCEPTION ) {
	CORBA_exception_free( &ipc_env );
	L4_KDB_Enter( "PCI forward failure" );
    }
}

