/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/kaxen/debug.cc
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

#include INC_ARCH(cpu.h)
#include INC_ARCH(cycles.h)
#include INC_ARCH(debug.h)

#include INC_WEDGE(cpu.h)
#include <console.h>
#include <debug.h>
#include INC_WEDGE(xen_hypervisor.h)
#include INC_WEDGE(memory.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(wedge.h)
#include INC_WEDGE(backend.h)

#include <burn_counters.h>
#include <profile.h>

// XXX no functionality
word_t irq_traced = 0;

static xen_frame_t *frame;

static u8_t debug_stack[PAGE_SIZE] ALIGNED(CONFIG_STACK_ALIGN);
static const word_t debug_stack_top = (word_t(debug_stack) + sizeof(debug_stack) - CONFIG_STACK_SAFETY) & ~(CONFIG_STACK_ALIGN-1);

struct dbg_cmd_group_t;

enum dbg_action_e {dbg_normal_action, dbg_quit_action};
typedef dbg_action_e (*dbg_cmd_func_t)( dbg_cmd_group_t * );

struct dbg_cmd_t
{
    enum {dbg_null_type=0, dbg_func_type, dbg_menu_type} type;
    char key;
    char *description;
    dbg_cmd_func_t func;
    dbg_cmd_group_t *menu;
};

struct dbg_cmd_group_t
{
    char *description;
    dbg_cmd_t *cmds;
};

extern dbg_cmd_group_t dbg_main_menu;
extern dbg_cmd_group_t dbg_arch_menu;

#define DBG_FUNC(name) dbg_action_e name (dbg_cmd_group_t *menu)

DBG_FUNC(dbg_quit);
DBG_FUNC(dbg_pdir_dump);
DBG_FUNC(dbg_frame_dump);
DBG_FUNC(dbg_mem_dump);
DBG_FUNC(dbg_shutdown);
DBG_FUNC(dbg_reboot);
DBG_FUNC(dbg_help);
DBG_FUNC(dbg_time);
DBG_FUNC(dbg_single_step);
#if defined(CONFIG_BURN_COUNTERS)
DBG_FUNC(dbg_burn_counters_dump);
#endif
#if defined(CONFIG_BURN_PROFILE)
DBG_FUNC(dbg_burn_profile_dump);
#endif
#if defined(CONFIG_INSTR_PROFILE)
DBG_FUNC(dbg_instr_profile_dump);
#endif
DBG_FUNC(dbg_dump_gdt);
DBG_FUNC(dbg_dump_idt);
DBG_FUNC(dbg_slow_int_perf);
DBG_FUNC(dbg_fast_int_perf);
DBG_FUNC(dbg_esp0_perf);
DBG_FUNC(dbg_pgfault_perf);

static dbg_cmd_t main_menu_cmds[] = {
    { dbg_cmd_t::dbg_func_type, 'g', "Exit debugger (go)", dbg_quit },
    { dbg_cmd_t::dbg_func_type, 'p', "Print the active page directory", dbg_pdir_dump },
    { dbg_cmd_t::dbg_func_type, 'f', "Print the interrupted context", dbg_frame_dump },
    { dbg_cmd_t::dbg_func_type, 'd', "dump memory", dbg_mem_dump },
    { dbg_cmd_t::dbg_func_type, 't', "current time", dbg_time },
    { dbg_cmd_t::dbg_func_type, '6', "Reboot the virtual machine", dbg_reboot },
    { dbg_cmd_t::dbg_func_type, '7', "Shutdown the virtual machine", dbg_shutdown },
    { dbg_cmd_t::dbg_func_type, 's', "Single step", dbg_single_step },
    { dbg_cmd_t::dbg_func_type, 'h', "Debugger help", dbg_help },
    { dbg_cmd_t::dbg_func_type, '?', "Debugger help", dbg_help },
    { dbg_cmd_t::dbg_menu_type, 'a', "Arch menu", 0, &dbg_arch_menu },
#if defined(CONFIG_BURN_COUNTERS)
    { dbg_cmd_t::dbg_func_type, 'b', "Print and reset burn counters", dbg_burn_counters_dump },
#endif
#if defined(CONFIG_BURN_PROFILE)
    { dbg_cmd_t::dbg_func_type, 'B', "Print the burn profile counters", dbg_burn_profile_dump },
#endif
#if defined(CONFIG_INSTR_PROFILE)
    { dbg_cmd_t::dbg_func_type, 'i', "Print and reset the instruction profile", dbg_instr_profile_dump },
#endif
    { dbg_cmd_t::dbg_null_type, 0, 0, 0 }
};

static dbg_cmd_t arch_menu_cmds[] = {
    { dbg_cmd_t::dbg_func_type, 'i', "Dump guest's IDT", dbg_dump_idt },
    { dbg_cmd_t::dbg_func_type, 'g', "Dump guest's GDT", dbg_dump_gdt },
    { dbg_cmd_t::dbg_func_type, 's', "Time the slow system call", dbg_slow_int_perf },
    { dbg_cmd_t::dbg_func_type, 'f', "Time the fast system call", dbg_fast_int_perf },
    { dbg_cmd_t::dbg_func_type, '0', "Time the esp0 system call", dbg_esp0_perf },
    { dbg_cmd_t::dbg_func_type, 'p', "Time page fault handling", dbg_pgfault_perf },
    { dbg_cmd_t::dbg_func_type, 'h', "Debugger help", dbg_help },
    { dbg_cmd_t::dbg_func_type, '?', "Debugger help", dbg_help },
    { dbg_cmd_t::dbg_menu_type, '\e', "Main menu", 0, &dbg_main_menu },
    { dbg_cmd_t::dbg_null_type, 0, 0, 0 }
};

dbg_cmd_group_t dbg_main_menu = {"main", main_menu_cmds};
dbg_cmd_group_t dbg_arch_menu = {"arch", arch_menu_cmds};


INLINE u8_t get_key()
{
    char key;

    if( xen_start_info.is_privileged_dom() )
      while( XEN_console_io(CONSOLEIO_read, 1, &key) != 1 );
    else
      key = xen_controller.console_destructive_read();

    return key;
}

INLINE void print_key( u8_t key )
{
    if( key == '\e' )
	printf( "ESC");
    else
	printf( "%c", char(key) );
}

inline word_t get_hex (const char * prompt, const word_t defnum = 0, const char * defstr = NULL)
{
    word_t num = 0;
    word_t len = 0;
    char c, r;

    if (prompt)
    {
	if (defstr)
	    printf( "%s[%s]: ", prompt, defstr );
	else
	    printf( "%s[%lu]: ", prompt, defnum );
    }

    while (len < (sizeof (word_t) * 2))
    {
	switch (r = c = get_key ())
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    num *= 16;
	    num += c - '0';
	    printf( "%c", r );
	    len++;
	    break;

	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	    c += 'a' - 'A';
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	    num *= 16;
	    num += c - 'a' + 10;
	    printf( "%c", r );
	    len++;
	    break;

	case 'x': case 'X':
	    // Allow "0x" prefix
	    if (len == 1 && num == 0)
	    {
	        printf( "%c", r );
		len--;
	    }
	    break;

	case '\b':
	    // Backspace
	    if (len > 0)
	    {
		printf( "\b \b");
		num /= 16;
		len--;
	    }
	    break;

	case 'n':
	case 'r':
	    if (len == 0)
	    {
		// Use default value
		if (defstr)
		    printf( "%s\n", defstr );
		else
		    printf( "%u\n", defnum );
		return defnum;
	    }
	    len = sizeof (word_t) * 2;
	    break;

	case '\e':
	    printf( "n" );
	    return ~0U;
	}
    }

    printf( "\n" );
    return num;
}



void debugger_enter( xen_frame_t *callback_frame )
{
    // Disable Xen callbacks, so that we don't compete for events. 
    volatile u8_t &xen_upcall_mask = 
#ifdef CONFIG_XEN_2_0
	xen_shared_info.vcpu_data[0].evtchn_upcall_mask;
#else
	xen_shared_info.vcpu_info[0].evtchn_upcall_mask;
#endif
    bool was_upcall_masked = bit_test_and_set_atomic( 0, xen_upcall_mask );
 
    frame = callback_frame;
    dbg_cmd_group_t *menu = &dbg_main_menu;

    word_t ip = frame ? frame->iret.ip : (word_t)__builtin_return_address(0);
    printf( "\n--- Afterburner debugger --- ip %p ---\n", ip );

    if( frame ) {
	// Disable single step.
	frame->iret.flags.x.fields.rf = 0;
	frame->iret.flags.x.fields.tf = 0;
    }

    bool finished = false;
    while( !finished )
    {
	printf( "%s ? ", menu->description );
	char key = get_key();
	bool handled = false;
	for( word_t i = 0; menu->cmds[i].type != dbg_cmd_t::dbg_null_type; i++ )
	{
	    if( menu->cmds[i].key != key )
		continue;
	    print_key( key );
	    printf( " - %s\n", menu->cmds[i].description );

	    if( menu->cmds[i].type == dbg_cmd_t::dbg_func_type ) {
		if( menu->cmds[i].func(menu) == dbg_quit_action )
		    finished = true;
	    }
	    else
		menu = menu->cmds[i].menu;

	    handled = true;
	    break;
	}

	if( !handled ) {
	    print_key( key );
	    printf( "\n" );
	}
    }

    // Enable Xen callbacks;
    if( !was_upcall_masked )
	xen_upcall_mask = 0;
}

DBG_FUNC(dbg_quit)
{
    return dbg_quit_action;
}

DBG_FUNC(dbg_pdir_dump)
{
    xen_memory.dump_active_pdir();
    return dbg_normal_action;
}

DBG_FUNC(dbg_frame_dump)
{
    if( !frame )
	return dbg_normal_action;

    printf( "ip: %p cs: %lu\n", frame->iret.ip, frame->iret.cs );
    if( frame->get_privilege() > 1 )
	printf( "sp: %x ss %lu", frame->iret.sp, frame->iret.ss );
    else {
	// A kernel frame is missing sp and ss.
	word_t sp = word_t(frame) + sizeof(frame) - 2*sizeof(word_t);
	printf( "sp: %p\n",sp );
    }
    printf( "flags: %lx\n", frame->iret.flags.x.raw );
    printf( "eax: %lx ebx: %lx ecx: %lx\n", frame->eax, frame->ebx,
            frame->ecx );
    printf( "edx: %lx esi: %lx edi: %lx\n", frame->edx, frame->esi,
	    frame->edi );
    printf( "ebp: %p\n", frame->ebp );
    printf( "frame id: %u", frame->get_id() );
    if( frame->uses_error_code() )
	printf( ", error code: %lx\n", frame->info.error_code );
    else
	printf( "\n" );
    if( frame->is_page_fault() )
	printf( "fault addr: %p\n", frame->info.fault_vaddr );

    return dbg_normal_action;
}

void memdump (u32_t addr)
{
    for (int j = 0; j < 16; j++)
    {
	printf( "%p  ", addr );
	u32_t *x = (u32_t *) addr;
	for (int i = 0; i < 4; i++)
	    printf( "%lx ", x[i] );

	u8_t * c = (u8_t *) addr;
	printf( "  ");
	for (int i = 0; i < 16; i++)
	{
	    if (i == 8) printf( " " );
	    printf( "%c", (((c[i] >= 32 && c[i] < 127) ||
		     (c[i] >= 161 && c[i] <= 191) ||
		     (c[i] >= 224)) ? (char)c[i] : '.') );
	}
	printf( "\n" );
	addr+= 16;
    }
}

void memdump_loop (u32_t addr)
{
    do {
	memdump (addr);
	addr += 16*16;
	printf( "Continue/Quit?\n");
    } while (get_key() != 'q');
}



DBG_FUNC (dbg_mem_dump)
{
    static u32_t kdb_last_dump;
    u32_t addr = get_hex ("Dump address", kdb_last_dump);

    if (addr == ~0U)
	return dbg_normal_action;

    kdb_last_dump = addr;
    memdump_loop (addr);

    return dbg_normal_action;
}

DBG_FUNC(dbg_shutdown)
{
#if defined(CONFIG_DEVICE_PASSTHRU)
    printf( "You will shutdown the real machine.  Are you sure? Y=yes ");
    char c = get_key();
    printf( "\n" );
    if( c == 'Y' )
#endif
    {
	XEN_shutdown( SHUTDOWN_poweroff );
    }
    return dbg_normal_action;
}

DBG_FUNC(dbg_reboot)
{
    XEN_shutdown( SHUTDOWN_reboot );
    return dbg_normal_action;
}

DBG_FUNC(dbg_help)
{
    for( word_t i = 0; menu->cmds[i].type != dbg_cmd_t::dbg_null_type; i++ )
    {
	print_key( menu->cmds[i].key );
	printf( " - %s\n", menu->cmds[i].description );
    }
    return dbg_normal_action;
}

DBG_FUNC(dbg_time)
{
    static const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday"};

    time_t unix_seconds = backend_get_unix_seconds();
    word_t year, month, day, hours, minutes, seconds;
    unix_to_gregorian( unix_seconds, year, month, day, hours, minutes, seconds);
    word_t week_day = day_of_week( year, month, day );

    printf( "%s ", days[week_day] );
    printf( "%u/%u/%u %u:%u:%u UTC\n",
            year, month, day, hours, minutes, seconds );

    return dbg_normal_action;
}

DBG_FUNC(dbg_single_step)
{
    frame->iret.flags.x.fields.rf = 1;
    frame->iret.flags.x.fields.tf = 1;
    return dbg_quit_action;
}

#if defined(CONFIG_BURN_COUNTERS)
DBG_FUNC(dbg_burn_counters_dump)
{
    burn_counters_dump();
    return dbg_normal_action;
}
#endif

#if defined(CONFIG_BURN_PROFILE)
DBG_FUNC(dbg_burn_profile_dump)
{
    extern word_t _burn_profile_start[];
    extern word_t _burn_profile_end[];
    word_t *counter;

    for( counter = _burn_profile_start; counter < _burn_profile_end; counter++ )
    {
	printf( "%lu\n", *counter );
    }
    return dbg_normal_action;
}
#endif

#if defined(CONFIG_INSTR_PROFILE)
DBG_FUNC(dbg_instr_profile_dump)
{
    return dbg_normal_action;
}
#endif

DBG_FUNC(dbg_dump_gdt)
{
    dump_gdt( get_cpu().gdtr );
    return dbg_normal_action;
}


DBG_FUNC(dbg_dump_idt)
{
    dump_idt( get_cpu().idtr );
    return dbg_normal_action;
}


extern "C" void dbg_int_perf_iret();
extern "C" void dbg_int_perf_finish();
extern "C" void dbg_int_perf_user();

#define INT_PERF_ITERATIONS	10000

static void dbg_int_perf( bool fast )
{
    static trap_info_t dbg_trap_table[] = {
	{ 0x20, 3, XEN_CS_KERNEL, (word_t)dbg_int_perf_iret },
	{ 0x21, 3, XEN_CS_KERNEL, (word_t)dbg_int_perf_finish },
	{ 0, 0, 0 }
    };
    vcpu_t &vcpu = get_vcpu();

    // Make the test code accessible to user.
    word_t start_page = word_t(dbg_int_perf_user) & PAGE_MASK;
    word_t end_page = word_t(dbg_int_perf_finish) & PAGE_MASK;
    for( word_t page = start_page; page <= end_page; page++ ) {
	pgent_t pgent = xen_memory.get_pgent( page );
	pgent.set_user();
	int err = XEN_update_va_mapping( page, pgent.get_raw(),
		UVMF_INVLPG );
	if( err )
	    PANIC( "Unable to update debug mapping." );
    }

    if( XEN_set_trap_table(dbg_trap_table) ) {
	printf( "Error: unable to install a new Xen trap table.\n");
	return;
    }
#ifdef CONFIG_XEN_2_0
    if( XEN_set_fast_trap(fast ? 0x20:0x21) )
	PANIC( "Unable to activate the fast trap." );
#endif

    vcpu.xen_esp0 = 0; // Invalidate the cached esp0.
    if( XEN_stack_switch(XEN_DS_KERNEL, debug_stack_top) )
	PANIC( "Xen stack switch failure." );

    u32_t start_upper, start_lower;
    cycles_t start_time, end_time;
    word_t dummy, dummy2;
    __asm__ __volatile__ (
	"	pushl	%%ebp ;"	// Preserve ebp.
	"	pushl	%%ecx ;"	// Preserve XEN_DS_KERNEL.
	"	pushl	%%ebx ;"	// SS
	"	pushl	%%esp ;"	// Use our stack.
	"	pushf;"			// Use our flags.
	"	pushl	%%eax ;"	// CS
	"	pushl	$dbg_int_perf_user ;"	// The code to execute at user.

	"dbg_int_perf_iret:"
	"	iret ;"

	"dbg_int_perf_user:"
	"	mov	$"MKSTR(INT_PERF_ITERATIONS)", %%ecx ;"
	"	rdtsc ;"
	"	movl	%%eax, %%esi ;"
	"	movl	%%edx, %%edi ;"
	"1:"
	"	int	$0x20 ;"	// Perform a NULL syscall.
	"	dec	%%ecx ;"
	"	jnz	1b ;"
	"	rdtsc ;"
	"	int	$0x21 ;"	// Exit the user test.

	"dbg_int_perf_finish:"
	"	mov	12(%%esp), %%esp ;"	// Restore the stack.
	"	popl	%%ebx ;"		// Remove SS.
	"	popl	%%ebx ;"		// Retrieve XEN_DS_KERNEL
	"	mov	%%ebx, %%ds ;"
	"	mov	%%ebx, %%es ;"
	"	popl	%%ebp ;"		// Restore ebp.

	: /* outputs */
	  "=A" (end_time), "=D" (start_upper), "=S" (start_lower),
	  "=b" (dummy), "=c" (dummy2)
	: /* inputs */
	  "a" (XEN_CS_USER), "b" (XEN_DS_USER), "c" (XEN_DS_KERNEL)
	: /* clobbers */
	  "memory", "flags"
	);

    // Remap the test code with privileged access.
    for( word_t page = start_page; page <= end_page; page++ ) {
	pgent_t pgent = xen_memory.get_pgent( page );
	pgent.set_kernel();
	int err = XEN_update_va_mapping( page, pgent.get_raw(),
		UVMF_INVLPG );
	if( err )
	    PANIC( "Unable to update debug mapping." );
    }

    // Restore the original trap table.
    init_xen_traps();

    start_time = start_upper;
    start_time = (start_time << 32) | start_lower;

    printf( "Total cycles: %lu\n", end_time - start_time );
    printf( "Total iterations: %lu\n", INT_PERF_ITERATIONS );
    printf( "Cycles per iteration: %lu.%lu%lu\n",
	    (end_time-start_time)/INT_PERF_ITERATIONS,
	    ((end_time-start_time) % INT_PERF_ITERATIONS) / (INT_PERF_ITERATIONS/10),
	    ((end_time-start_time) % (INT_PERF_ITERATIONS/10)) / (INT_PERF_ITERATIONS/100) );

}

DBG_FUNC(dbg_slow_int_perf)
{
    dbg_int_perf( false );
    return dbg_normal_action;
}

DBG_FUNC(dbg_fast_int_perf)
{
    dbg_int_perf( true );
    return dbg_normal_action;
}

DBG_FUNC(dbg_esp0_perf)
{
    static const word_t iterations = 10000;

    get_vcpu().xen_esp0 = 0; // Invalidate the cached esp0.
    word_t esp0 = debug_stack_top;

    cycles_t start_time = get_cycles();
    for( word_t i = 0; i < iterations; i++ )
	XEN_stack_switch( XEN_DS_KERNEL, esp0 );
    cycles_t end_time = get_cycles();

    printf( "Total cycles: %lu\n", end_time - start_time );
    printf( "Total iterations: %lu\n", iterations );
    printf( "Cycles per iteration: %lu.%lu%lu" ,
	    (end_time-start_time)/iterations,
	    ((end_time-start_time) % iterations) / (iterations/10),
	    ((end_time-start_time) % (iterations/10)) / (iterations/100) );

    return dbg_normal_action;
}

DBG_FUNC(dbg_pgfault_perf)
{
    static const word_t iterations = 100000;
    static const bool perms_pgfault = true;

    word_t fault_addr = 0;
    if( perms_pgfault )
	fault_addr = word_t(mapping_base_region);
    else {
	bool found = false;
	for( word_t j = 0; !found && j < 1024; j++, fault_addr += PAGEDIR_SIZE )
	    if( !xen_memory.get_pdent(fault_addr).is_valid() )
		found = true;

	if( !found ) {
    	    printf( "Unable to find a suitable fault address.\n");
    	    return dbg_normal_action;
       	}
    }

    cycles_t start_time = get_cycles();
    for( word_t i = 0; i < iterations; i++ )
    {
	__asm__ __volatile__ (
	    // Cause a write fault.
	    ".global dbg_pgfault_perf_fault, dbg_pgfault_perf_resume;"
	    "dbg_pgfault_perf_fault:"
	    "	mov %%eax, (%%ebx) ;"
	    "dbg_pgfault_perf_resume:"
	    : : "b"(fault_addr)
	    );
    }
    cycles_t end_time = get_cycles();

    printf( "Total cycles: %lu\n", end_time - start_time );
    printf( "Total iterations: %lu\n", iterations );
    printf( "Cycles per iteration: %lu.%lu%lu" ,
	    (end_time-start_time)/iterations,
	    ((end_time-start_time) % iterations) / (iterations/10),
	    ((end_time-start_time) % (iterations/10)) / (iterations/100) );

    return dbg_normal_action;
}

bool dbg_pgfault_perf_resolve( xen_frame_t *pgfault_frame )
{
    extern word_t dbg_pgfault_perf_fault[];
    extern word_t dbg_pgfault_perf_resume[];

    if( pgfault_frame->iret.ip == (word_t)dbg_pgfault_perf_fault )
    {
	pgfault_frame->iret.ip = (word_t)dbg_pgfault_perf_resume;
	return true;
    }

    return false;
}

