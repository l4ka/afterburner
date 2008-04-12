/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 * Copyright (C) 2005,  Volkmar Uhlig
 *
 * File path:     afterburn-wedge/kaxen/callbacks.cc
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
#include INC_ARCH(intlogic.h)
#include INC_ARCH(bitops.h)

#include INC_WEDGE(xen_hypervisor.h)
#include INC_WEDGE(cpu.h)
#include <console.h>
#include <debug.h>
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(wedge.h)

#include <burn_counters.h>

static const bool debug_device_irq=0;

DECLARE_BURN_COUNTER(async_delivery);
DECLARE_BURN_COUNTER(async_delivery_canceled);
DECLARE_BURN_COUNTER(async_restart);
DECLARE_BURN_COUNTER(device_unmask);
DECLARE_BURN_COUNTER(device_irq);
DECLARE_BURN_COUNTER(device_irq_idle);
DECLARE_BURN_COUNTER(idle_fixup);
DECLARE_BURN_COUNTER(iret_irq_fixup);
DECLARE_BURN_COUNTER_REGION(callback_irq_raise, 16);
DECLARE_BURN_COUNTER(timer_overlap);

extern void serial8250_receive_byte( u8_t byte );
extern "C" void xen_event_callback_wrapper();
extern "C" void xen_event_failsafe_wrapper();

#if defined(CONFIG_XEN_2_0)
static volatile u8_t &xen_upcall_pending = 
	xen_shared_info.vcpu_data[0].evtchn_upcall_pending;
static volatile u8_t &xen_upcall_mask = 
	xen_shared_info.vcpu_data[0].evtchn_upcall_mask;
static volatile u32_t &xen_evtchn_pending = 
	xen_shared_info.evtchn_pending[0];
static volatile u32_t &xen_evtchn_pending_sel = 
	xen_shared_info.evtchn_pending_sel;
#else
static volatile u8_t &xen_upcall_pending = 
	xen_shared_info.vcpu_info[0].evtchn_upcall_pending;
static volatile u8_t &xen_upcall_mask = 
	xen_shared_info.vcpu_info[0].evtchn_upcall_mask;
static volatile word_t &xen_evtchn_pending = 
	((word_t *)xen_shared_info.evtchn_pending)[0];
static volatile unsigned long &xen_evtchn_pending_sel = 
	xen_shared_info.vcpu_info[0].evtchn_pending_sel;
#endif

static const word_t max_channels = 32;
word_t channel_to_irq[ max_channels ];
static const word_t max_irqs = 256;
word_t irq_to_channel[ max_irqs ];

#ifndef CONFIG_ARCH_AMD64
static burn_redirect_frame_t *idle_redirect = NULL;
#endif

INLINE void channel_unmask( word_t channel )
{
    volatile word_t &word = ((word_t *)xen_shared_info.evtchn_mask)[ channel/(sizeof(word_t)*8) ];
    word_t bit = channel % (sizeof(word_t)*8);
    bit_clear_atomic( bit, word );
}

INLINE void channel_mask( word_t channel )
{
    volatile word_t &word = ((word_t *)xen_shared_info.evtchn_mask)[ channel/(sizeof(word_t)*8) ];
    word_t bit = channel % (sizeof(word_t)*8);
    bit_set_atomic( bit, word );
}

INLINE bool channel32_pending( word_t channel )
{
    return bit_test_and_clear_atomic(channel, xen_evtchn_pending);
}

static const word_t channel_nil = ~0ul;
static word_t channel_timer = 0;
static word_t channel_console = channel_nil;
static word_t channel_controller = channel_nil;

static cycles_t cycles_10ms = 0;
static cycles_t last_timer_cycles = 0;

static word_t xen_bind_irq (word_t irq, bool virt)
{
    evtchn_op_t op;
    if (virt) {
	op.cmd = EVTCHNOP_bind_virq;
	op.u.bind_virq.virq = irq;
    } else {
	op.cmd = EVTCHNOP_bind_pirq;
	op.u.bind_pirq.pirq = irq;
	op.u.bind_pirq.flags = 0;
    }

#ifdef CONFIG_XEN_3_0
    op.u.bind_virq.vcpu = get_vcpu().get_id();
#endif
    if( XEN_event_channel_op(&op) )
	PANIC( "Failed to attach to Xen interrupt source." );
    return (virt) ? op.u.bind_virq.port : op.u.bind_pirq.port;
}

#ifdef CONFIG_ARCH_AMD64
void backend_update_syscall_entry( word_t addr )
{
    get_cpu().syscall_entry = addr;
    if( XEN_set_callbacks((word_t)xen_event_callback_wrapper,
	    (word_t)xen_event_failsafe_wrapper, addr ) )
	PANIC( "Failed to initialize the Xen callbacks." );
}
#endif

void init_xen_callbacks()
{
#ifdef CONFIG_ARCH_AMD64
    /* XXX the pointer won't be initialized correctly if this is removed? */
    printf("%p %p\n", &xen_upcall_mask, &xen_shared_info.vcpu_info[0].evtchn_upcall_mask);
#endif

    xen_upcall_mask = 1;

#ifdef CONFIG_ARCH_IA32
    word_t cs = XEN_CS_KERNEL;
    if( XEN_set_callbacks(cs, (word_t)xen_event_callback_wrapper,
	    cs, (word_t)xen_event_failsafe_wrapper) )
#else
    if( XEN_set_callbacks((word_t)xen_event_callback_wrapper,
	    (word_t)xen_event_failsafe_wrapper, 0 ) )
#endif
	PANIC( "Failed to initialize the Xen callbacks." );

    // How many cycles executed by this CPU in 10ms?
    cycles_10ms = get_vcpu().cpu_hz;
    cycles_10ms = 10*cycles_10ms / 1000;

    // Attach to the timer VIRQ.
    channel_timer = xen_bind_irq(VIRQ_TIMER, true);
 
    // Attach to the console VIRQ.
    if( xen_start_info.is_privileged_dom() )
	channel_console = xen_bind_irq(VIRQ_CONSOLE, true);
#if defined(CONFIG_XEN_2_0)
    else
	channel_controller = xen_start_info.domain_controller_evtchn;
#endif

    // Enable upcalls.
    xen_upcall_mask = 0;

    // Unmask event channels.
    if( channel_nil != channel_console )
	channel_unmask( channel_console );
    channel_unmask( channel_timer );
}

void SECTION(".text.irq") xen_deliver_irq( xen_frame_t *frame )
{
    intlogic_t &intlogic = get_intlogic();
    if( !intlogic.maybe_pending_vector() )
	return;

    bool saved_int_state = get_cpu().disable_interrupts();
    if( !saved_int_state ) {
	// Interrupts were disabled.
	return;
    }

    // Now that interrupts are disabled, future callbacks won't attempt
    // to also deliver interrupts; they'll resume execution at this point.

    word_t vector, irq;
    if( !intlogic.pending_vector(vector, irq) ) {
	// Another callback or CPU claimed the interrupt.  None left to deliver.
	if( saved_int_state )
	    get_cpu().restore_interrupts( saved_int_state );
	return;
    }

#ifdef CONFIG_ARCH_AMD64
    UNIMPLEMENTED();
#else
    xen_deliver_async_vector( vector, frame, false, saved_int_state );
#endif
}

INLINE bool iret_race_fixup( xen_frame_t *frame )
{
#if defined(CONFIG_IA32_FAST_VECTOR)
    extern word_t burn_iret_restart[], burn_iret_end[];
    if( (frame->iret.ip >= word_t(burn_iret_restart))
    	    && (frame->iret.ip < word_t(burn_iret_end)) )
    {
	frame->iret.ip = word_t(burn_iret_restart);
	INC_BURN_COUNTER(iret_irq_fixup);
	return true;
    }
#endif
    return false;
}

INLINE bool at_idle( xen_frame_t *frame )
{
    /* idle_race_fixup() sets the IP to idle_race_end, so we compare
     * to <= idle_race_end.
     */
    extern word_t idle_race_start[], idle_race_end[];
    return (frame->iret.ip >= word_t(idle_race_start))
	    && (frame->iret.ip <= word_t(idle_race_end));
}

INLINE void idle_race_fixup( xen_frame_t *frame )
{
#ifndef CONFIG_ARCH_AMD64
    extern word_t idle_race_start[], idle_race_end[];
    if(EXPECT_FALSE( (frame->iret.ip >= word_t(idle_race_start))
	    && (frame->iret.ip < word_t(idle_race_end)) ))
    {
	INC_BURN_COUNTER(idle_fixup);
	frame->iret.ip = word_t(idle_race_end);
    }
#endif
}

INLINE bool async_safe( xen_frame_t *frame )
{
    return (frame->iret.ip < CONFIG_WEDGE_VIRT);
}

#ifdef CONFIG_ARCH_IA32
extern "C" void __attribute__(( regparm(1) )) SECTION(".text.irq")
#else
extern "C" void SECTION(".text.irq")
#endif
xen_event_callback( xen_frame_t *frame )
{
    intlogic_t &intlogic = get_intlogic();
    ASSERT( xen_upcall_mask );
    bool timer_fired = false;
    bool device_fired = false;

    idle_race_fixup( frame );

restart:
    while( xen_upcall_pending )
    {
	xen_upcall_pending = 0;
	xen_evtchn_pending_sel = 0;

	while( xen_evtchn_pending )
	{
	    word_t channel = lsb( xen_evtchn_pending );
	    if( !channel32_pending(channel) )
		continue;

	    if( channel == channel_timer ) {
		timer_fired = true;
    	    }

	    else if( channel == channel_console ) {
		char key;
		if( XEN_console_io(CONSOLEIO_read, 1, &key) == 1 ) {
#if defined(CONFIG_DEBUGGER)
		    if( CONFIG_DEBUGGER_BREAKIN_KEY == key )
			debugger_enter( frame );
		    else
#endif
		    {
		       	serial8250_receive_byte( (u8_t)key );
		    }
		}
	    }

	    else if( channel == channel_controller ) {
		xen_controller.process_async_event( frame );
	    }

	    else { // Handle physical device interrupts.
		// TODO: what if a pending channel isn't in the channel_to_irq
		// map, such as the timer channel?
		INC_BURN_COUNTER(device_irq);
		device_fired = true;
	    	if( channel_to_irq[channel] < 16 )
    		    INC_BURN_REGION_WORD(callback_irq_raise, channel_to_irq[channel]);
		intlogic.raise_irq( channel_to_irq[channel] );
	    }
	}
    }

    if( timer_fired && device_fired ) {
	INC_BURN_COUNTER(timer_overlap);
	if( at_idle(frame) ) {
	    INC_BURN_COUNTER(device_irq_idle);
	    // Xen sends a timer event when waking a domain.
	    // We don't know whether this timer event is our 10ms periodic
	    // timer.
	    cycles_t delta = get_cycles() - last_timer_cycles;
	    if( delta < cycles_10ms )
		timer_fired = false;
	}
    }
    if( timer_fired ) {
	last_timer_cycles = get_cycles();
	intlogic.raise_irq( 0 );
	INC_BURN_REGION_WORD(callback_irq_raise, 0);
    }

    if( at_idle(frame) ) {
	// It is only safe to touch the idle_redirect while at_idle().
	// In the case of recursive callbacks, only one interrupted frame 
	// can be at_idle() at a time.  But it is possible for the idle
	// loop to be interrupted multiple times, and thus to have
	// successive frames to be at_idle(), in which case we must not 
	// overwrite the prior redirect.
	// Race between is_redirect() and do_redirect() is protected by 
	// xen_upcall_mask.
#ifdef CONFIG_ARCH_AMD64
        UNIMPLEMENTED();
#else
	if( !idle_redirect->is_redirect() )
	    idle_redirect->do_redirect();
#endif
    }

    //printf(".");
    ASSERT( xen_upcall_mask );
    xen_upcall_mask = 0;
    if( xen_upcall_pending ) {
	INC_BURN_COUNTER(async_restart);
	xen_upcall_mask = 1;
	goto restart;
    }

    // After this point, new callbacks may arrive, cancelling any of the
    // following code.

    if( async_safe(frame) ) {
     	INC_BURN_COUNTER(async_delivery);
	xen_deliver_irq( frame );
    }
    else if( !iret_race_fixup(frame) )
	INC_BURN_COUNTER(async_delivery_canceled);
}

#ifdef CONFIG_ARCH_IA32
extern "C" void __attribute__(( regparm(1) ))
#else
extern "C" void
#endif
xen_event_failsafe( xen_frame_t *frame )
{
    PANIC( "Xen failsafe callback.  Don't know what to do.", frame );
}

#ifndef CONFIG_ARCH_AMD64
void backend_interruptible_idle( burn_redirect_frame_t *redirect_frame )
{
    ASSERT( get_cpu().interrupts_enabled() );
    if( redirect_frame->do_redirect() )
	return; // We must deliver an interrupt, so cancel the idle.
    idle_redirect = redirect_frame;

    cycles_t current_cycles = get_cycles();
    if( (current_cycles - last_timer_cycles) > cycles_10ms ) {
	// Missed timer.
	last_timer_cycles = current_cycles;
	get_intlogic().raise_irq( 0 );
	INC_BURN_REGION_WORD(callback_irq_raise, 0);
	if( !redirect_frame->is_redirect() )
	    redirect_frame->do_redirect();
	return;
    }

    u64_t time_ns = xen_shared_info.get_current_time_ns( 
	    cycles_10ms + last_timer_cycles );

    /* Xen has a silly race condition ... if the time-out event comes between
     * XEN_set_timer_op() and XEN_block(), then we'll block indefinitely.
     * Additionally, if a normal timer event fires between XEN_set_timer_op()
     * and XEN_block(), then we'll re-enter the guest kernel and potentially
     * miss the just-installed time-out, returning to an indefinite XEN_block().
     * (If XEN_block() accepted a time-out parameter, we wouldn't have this
     * problem, although Xen is preemptible and would still probably have a race
     * condition.  Xen solves the problem by having XenoLinux disable 
     * callbacks before calling XEN_block(), and then having 
     * XEN_block() enable callbacks.  I don't want to disable callbacks;
     * it is unnecessary.)
     */
    /* When a timer interrupts the following code block, we want the timer
     * interrupt to roll-forward the code, to avoid the race condition.  
     * Thus use assembler, rather than the compiler, to avoid corrupting 
     * compiler state while rolling forward.
     */
    /* Our logic also has a race condition: We check for pending interrupts,
     * and if there are no pending interrupts, we put the VM to sleep.  Our
     * race condition is that an interrupt can arrive between the 
     * pending interrupt check and the sleep hypercall.  Such an interrupt
     * must prevent us from putting the VM to sleep.  The roll-forward
     * code protects against this race condition.
     */
//#ifndef CONFIG_XEN_2_0
    // Seems that Xen 3 requires that we enter XEN_block() without pending
    // callbacks (or perhaps without a pending timer callback).  Ugh.
    xen_upcall_mask = 1;
    if( xen_do_callbacks() ) {
	if( !idle_redirect->is_redirect() )
    	    idle_redirect->do_redirect();
    }
    if( idle_redirect->is_redirect() ) {
	xen_upcall_mask = 0;
	return;
    }
//#endif

#if OFS_INTLOGIC_VECTOR_CLUSTER != 0
#error "OFS_INTLOGIC_VECTOR_CLUSTER != 0"
#endif
    INC_BURN_COUNTER(XEN_set_timer_op);
    ON_BURN_COUNTER(cycles_t cycles = get_cycles());
    u32_t *time_val_words = (u32_t *)&time_ns;

    word_t a, b, c;
    __asm__ __volatile__ (
	    ".global idle_race_start, idle_race_end\n"
    	    "idle_race_start:\n"	/* From here, roll forward. */
	    "cmp $0, 0 + intlogic ;"	/* Pending interrupts? */
	    "jnz idle_race_end ;"	/* Abort to handle interrupt. */
    	    TRAP_INSTR ";"		/* Invoke XEN_set_timer_op(). */
	    "mov %6, %%eax ;"		/* Prepare for XEN_block(). */
	    "mov %7, %%ebx ;"		/* Prepare for XEN_block(). */
#ifndef CONFIG_XEN_2_0
	    "mov $0, %%ecx ;"		/* Prepare for XEN_block(). */
#endif
	    TRAP_INSTR ";"		/* Invoke XEN_block(). */
    	    "idle_race_end:\n"		/* Roll forward destination. */
    	    : /* outputs */
	    "=a" (a), "=b" (b), "=c" (c)
    	    : /* inputs */
	    "0" (__HYPERVISOR_set_timer_op),
#ifdef CONFIG_XEN_2_0
	    "1" (time_val_words[1]), "2" (time_val_words[0]),
#else
	    "1" (time_val_words[0]), "2" (time_val_words[1]),
#endif
	    "i" (__HYPERVISOR_sched_op), "i" (SCHEDOP_block)
    	    : /* clobbers */
  	    "flags", "memory"
	);
    ADD_PERF_COUNTER(XEN_set_timer_op_cycles, get_cycles() - cycles);

    /* If we had an interrupt prior to idle_race_start, but after the
     * last check, then we need to fill out the redirect frame.
     */
    if( !idle_redirect->is_redirect() )
	idle_redirect->do_redirect();
}
#else
char idle_race_start, idle_race_end;
#endif

bool backend_enable_device_interrupt( u32_t interrupt, vcpu_t& unused )
{
    if( debug_device_irq )
	printf( "Request to enable interrupt %u\n", interrupt );
    if( interrupt >= max_irqs )
	PANIC( "Interrupt request out of range for interrupt %u\n",
	       interrupt );

    word_t channel = xen_bind_irq(interrupt, false);

    if( debug_device_irq )
	printf( "Interrupt %u is on channel %lu\n",
	       interrupt, channel );
    if( channel >= max_channels )
	PANIC( "Xen assigned an out-of-range port %lu", channel );

    channel_to_irq[ channel ] = interrupt;
    irq_to_channel[ interrupt ] = channel;

    return true;
}

bool backend_unmask_device_interrupt( u32_t interrupt )
{
    INC_BURN_COUNTER(device_unmask);
    if( debug_device_irq )
	printf( "Request to unmask interrupt %u", interrupt );

    channel_unmask( irq_to_channel[interrupt] );

    // TODO: figure out how this works.  Shouldn't we have an unmask
    // for a particular physical irq, rather than a general unmask?
    physdev_op_t op;
    op.cmd = PHYSDEVOP_IRQ_UNMASK_NOTIFY;
    if( XEN_physdev_op(&op) ) {
	printf( "Error unmasking a Xen device interrupt.\n");
	return false;
    }
//    xen_do_callbacks();

    return true;
}
