/*********************************************************************
 *
 * Copyright (C) 2005-2006,  Volkmar Uhlig
 * Copyright (C) 2005-2006,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/vmi.cc
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

#include INC_ARCH(types.h)
#include INC_ARCH(cpu.h)
#include INC_ARCH(ops.h)

#include INC_WEDGE(console.h)
#include INC_WEDGE(backend.h)

#include <device/sequencing.h>
#include <device/portio.h>
#include <memory.h>

#include INC_ARCH(vmiTypes.h)
#include INC_ARCH(vmiRom.h)

static const bool debug_init = 0;
static const bool debug_rewrite = 0;
static const bool debug_phys2mach = 0;
static const bool debug_mach2phys = 0;

extern u8_t _VMI_ROM_START[], _VMI_ROM_END[], VMI_Init[];
extern trap_info_t xen_trap_table[];
extern "C" void trap_wrapper_13();

void
vmi_init( void )
{
    word_t rom_length = word_t(_VMI_ROM_END) - word_t(_VMI_ROM_START);

    VROMHeader *romstart = (VROMHeader*)CONFIG_VMI_ROM_START;
    con << "Initializing VMI ROM, size " << rom_length << '\n';
    memcpy( romstart, (void*)&_VMI_ROM_START, rom_length );
    romstart->romSignature = 0xaa55;
    romstart->romLength = rom_length / 512;
    if( rom_length % 512 )
	romstart->romLength++;
    romstart->vRomSignature = VMI_SIGNATURE;
    romstart->APIVersionMajor = VMI_API_REV_MAJOR;
    romstart->APIVersionMinor = VMI_API_REV_MINOR;

    con << "Relocating ROM from " << (void*)&_VMI_ROM_START 
	<< " to " << (void*)romstart << "\n";
}

void 
vmi_rewrite_calls_ext( burn_clobbers_frame_t *frame )
{
    extern u8_t _vmi_rewrite_calls_return[];
    extern word_t _vmi_patchups_start[], _vmi_patchups_end[];
    s32_t load_delta = (s32_t)(word_t(_vmi_rewrite_calls_return) - frame->burn_ret_address);
    s32_t reloc_delta = word_t(_VMI_ROM_START) - frame->eax;

    if( debug_rewrite )
	con << "ROM init link address: " << (void *)_vmi_rewrite_calls_return 
	    << ", new init address: " << (void *)frame->burn_ret_address
	    << ", ROM relocation: " << (void *)reloc_delta
	    << ", ROM load delta: " << (void *)load_delta << '\n';

    for( word_t *call_addr = _vmi_patchups_start;
	    call_addr < _vmi_patchups_end; call_addr++ )
    {
	u8_t *instr = (u8_t *)(*call_addr - load_delta);
	if( debug_rewrite )
	    con << "VMI call at " << (void *)*call_addr << "/"
		<< (void *)instr << ", relocated to " 
		<< (void *)(*call_addr - reloc_delta) << '\n';

	if( (instr[0] != OP_CALL_REL32) && (instr[0] != OP_JMP_REL32) )
	    PANIC( "Invalid VMI ROM instruction: " << (void *)(word_t)instr[0]);

	s32_t *call_target = (s32_t *)&instr[1];
	*call_target = *call_target + reloc_delta;
    }
}

extern "C" void vmi_init_ext( burn_clobbers_frame_t *frame )
{
    word_t guest_rom_start = frame->eax;

    con << "VMI init called from guest.  The guest relocated the ROM to "
	<< (void *)guest_rom_start << '\n';
    if( debug_init ) {
	con << "  from ROM @ " << (void *)frame->burn_ret_address << '\n';
	con << "  from guest @ " << (void *)frame->guest_ret_address << '\n';
    }

    /* hand back trap 13 */
    xen_trap_table[13].address = (word_t)trap_wrapper_13;

    if( XEN_set_trap_table(xen_trap_table) )
	PANIC( "Unable to configure the Xen trap table." );

    vmi_rewrite_calls_ext( frame );

    frame->eax = 0; // success
}

extern "C" void vmi_flush_tlb_ext ( burn_clobbers_frame_t *frame )
{
    backend_flush_old_pdir( get_cpu().cr3.get_pdir_addr(),
	    get_cpu().cr3.get_pdir_addr() );
}

extern "C" void
vmi_esp0_update_ext( burn_clobbers_frame_t *frame )
{
    backend_esp0_sync();
}


extern "C" void
vmi_phys_to_machine_ext( burn_clobbers_frame_t *frame )
{
#if defined(CONFIG_DEVICE_PASSTHRU)
    if (debug_phys2mach) con << "VMI_PhysToMachine: " << (void*)frame->eax;
    frame->eax = backend_phys_to_dma_hook( frame->eax, frame->edx );
    if (debug_phys2mach) con << " --> " << (void*)frame->eax << '\n';
#endif
}

extern "C" void
vmi_machine_to_phys_ext( burn_clobbers_frame_t *frame )
{
#if defined(CONFIG_DEVICE_PASSTHRU)
    if (debug_mach2phys) con << "VMI_MachineToPhys: " << (void*)frame->eax;
    frame->eax = backend_dma_to_phys_hook( frame->eax );
    if (debug_mach2phys) con << " --> " << (void*)frame->eax << '\n';
#endif
}

extern "C" void
vmi_phys_is_coherent_ext( burn_clobbers_frame_t *frame )
{
#if defined(CONFIG_DEVICE_PASSTHRU)
    frame->eax = backend_dma_coherent_check( frame->eax, frame->edx );
#else
    frame->eax = true;
#endif
}


/*
extern "C" void
vmi_cpu_stts( burn_clobbers_frame_t *frame )
{
    get_cpu().cr0.x.fields.ts = 1;
}
*/

extern "C" void 
vmi_cpu_shutdown( burn_clobbers_frame_t *frame )
{
    backend_shutdown();
}

extern "C" void 
vmi_cpu_reboot( burn_clobbers_frame_t *frame )
{
    backend_reboot();
}    

extern "C" void
vmi_get_cpu_hz( burn_clobbers_frame_t *frame )
{
    frame->eax = 0; //get_vcpu().cpu_hz;
    frame->edx = 0;
}

/**************************************************************************
 * Port string operations.
 **************************************************************************/

struct vmi_port_string_t {
    word_t burn_ret_address;
    word_t frame_pointer;
    word_t edx;
    word_t ecx;
    word_t eax;
    word_t addr;
    word_t guest_ret_address;
};

template <const word_t bitwidth, typename type>
INLINE void vmi_cpu_write_port_string( vmi_port_string_t *frame )
{
    u16_t port = frame->edx & 0xffff;
    while( frame->ecx-- ) {
	portio_write( port, *(type *)frame->addr, bitwidth );
	frame->addr += bitwidth / 8;
    }
}

extern "C" void vmi_cpu_write_port_string_8( vmi_port_string_t *frame ) {
    vmi_cpu_write_port_string<8, u8_t>(frame);
}
extern "C" void vmi_cpu_write_port_string_16( vmi_port_string_t *frame ) {
    vmi_cpu_write_port_string<16, u16_t>(frame);
}
extern "C" void vmi_cpu_write_port_string_32( vmi_port_string_t *frame ) {
    vmi_cpu_write_port_string<32, u32_t>(frame);
}


template <const word_t bitwidth, typename type>
INLINE void vmi_cpu_read_port_string( vmi_port_string_t *frame )
{
    u16_t port = frame->edx & 0xffff;
    while( frame->ecx-- ) {
	u32_t value;
	portio_read( port, value, bitwidth );
	*(type *)frame->addr = (type)value;
	frame->addr += bitwidth / 8;
    }
}

extern "C" void vmi_cpu_read_port_string_8( vmi_port_string_t *frame ) {
    vmi_cpu_read_port_string<8, u8_t>(frame);
}
extern "C" void vmi_cpu_read_port_string_16( vmi_port_string_t *frame ) {
    vmi_cpu_read_port_string<16, u16_t>(frame);
}
extern "C" void vmi_cpu_read_port_string_32( vmi_port_string_t *frame ) {
    vmi_cpu_read_port_string<32, u32_t>(frame);
}

/**************************************************************************/

#if 0
// Size of a call-relative instruction:
static const word_t call_relative_size = 5;

extern "C" void
vmi_rewrite_rdtsc_ext( burn_clobbers_frame_t *frame )
    // Rewrite the guest's rdtsc callsite to directly execute rdtsc.
{
    u8_t *opstream = (u8_t *)(frame->guest_ret_address - call_relative_size);
    if( opstream[0] != OP_CALL_REL32 )
	PANIC( "Unexpected call to the VMI ROM at " 
		<< (void *)frame->guest_ret_address );

    opstream[0] = OP_2BYTE;
    opstream[1] = OP_RDTSC;
    opstream[2] = OP_NOP1;
    opstream[3] = OP_NOP1;
    opstream[4] = OP_NOP1;

    // Restart the instruction.
    frame->guest_ret_address -= call_relative_size;
}
#endif
