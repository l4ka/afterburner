/*********************************************************************
 *                
 * Copyright (C) 2007,  Karlsruhe University
 *                
 * File path:     handle_cpuid.cc
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#include <l4/schedule.h>
#include <l4/ipc.h>
#include <string.h>
#include <string.h>
#include <l4/ipc.h>
#include <l4/schedule.h>
#include <l4/kip.h>
#include <l4/ia32/virt.h>
#include <device/portio.h>

#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(monitor.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(l4privileged.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(user.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(vm.h)
#include INC_WEDGE(vt/ia32.h)

INLINE u32_t word_text( char a, char b, char c, char d )
{
    return (a)<<0 | (b)<<8 | (c)<<16 | (d)<<24;
}

void backend_cpuid_override( 
	u32_t func, u32_t max_basic, u32_t max_extended,
	frame_t *regs )
{
    if( (func > max_basic) && (func < 0x80000000) )
	func = max_basic;
    if( func > max_extended )
	func = max_extended;

    if( func <= max_basic )
    {
	switch( func )
	{
	    case 1:
		bit_clear( 3, regs->x.fields.ecx ); // Disable monitor/mwait.
		break;
	}
    }
    else {
	switch( func )
	{
	    case 0x80000002:
		regs->x.fields.eax = word_text('L','4','K','a');
		regs->x.fields.ebx = word_text(':',':','P','i');
		regs->x.fields.ecx = word_text('s','t','a','c');
		regs->x.fields.edx = word_text('h','i','o',' ');
		break;
	    case 0x80000003:
		regs->x.fields.eax = word_text('(','h','t','t');
		regs->x.fields.ebx = word_text('p',':','/','/');
		regs->x.fields.ecx = word_text('l','4','k','a');
		regs->x.fields.edx = word_text('.','o','r','g');
		break;
	    case 0x80000004:
		regs->x.fields.eax = word_text(')',0,0,0);
		break;
	}
    }
}

void handle_cpuid( frame_t *frame )
{
    u32_t func = frame->x.fields.eax;
    static u32_t max_basic=0, max_extended=0;

    // Note: cpuid is a serializing instruction!

    if( max_basic == 0 )
    {
	// We need to determine the maximum inputs that this CPU permits.

	// Query for the max basic input.
	frame->x.fields.eax = 0;
	__asm__ __volatile__ ("cpuid"
    		: "=a"(frame->x.fields.eax), "=b"(frame->x.fields.ebx), 
		  "=c"(frame->x.fields.ecx), "=d"(frame->x.fields.edx)
    		: "0"(frame->x.fields.eax));
	max_basic = frame->x.fields.eax;

	// Query for the max extended input.
	frame->x.fields.eax = 0x80000000;
	__asm__ __volatile__ ("cpuid"
    		: "=a"(frame->x.fields.eax), "=b"(frame->x.fields.ebx),
		  "=c"(frame->x.fields.ecx), "=d"(frame->x.fields.edx)
    		: "0"(frame->x.fields.eax));
	max_extended = frame->x.fields.eax;

	// Restore the original request.
	frame->x.fields.eax = func;
    }

    // TODO: constrain basic functions to 3 if 
    // IA32_CR_MISC_ENABLES.BOOT_NT4 (bit 22) is set.

    // Execute the cpuid request.
    __asm__ __volatile__ ("cpuid"
	    : "=a"(frame->x.fields.eax), "=b"(frame->x.fields.ebx), 
	      "=c"(frame->x.fields.ecx), "=d"(frame->x.fields.edx)
	    : "0"(frame->x.fields.eax));

    // Start our over-ride logic.
    backend_cpuid_override( func, max_basic, max_extended, frame );
}
