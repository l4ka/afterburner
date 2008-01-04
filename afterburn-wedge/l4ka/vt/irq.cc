/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/irq.cc
 * Description:   The irq thread for handling asynchronous events.
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

#include <l4/ipc.h>
#include <l4/kip.h>
#include <l4/schedule.h>

#include INC_ARCH(intlogic.h)
#include <console.h>
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(l4privileged.h)
#include <device/acpi.h>
#include <device/i8253.h>
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(irq.h)
#include INC_WEDGE(message.h)
#include INC_WEDGE(vm.h)

static unsigned char irq_stack[KB(16)] ALIGNED(CONFIG_STACK_ALIGN);
static L4_Clock_t timer_length = {raw: 54925}; /* 18.2 HZ */
extern i8253_t i8253;


static void irq_handler_thread( void *param, hthread_t *hthread )
{
    L4_Word_t tid_user_base = L4_ThreadIdUserBase(L4_GetKernelInterface());

    printf( "Entering IRQ loop, TID %t\n", hthread->get_global_tid());
    
    vcpu_t &vcpu = get_vcpu();
    // Set our thread's exception handler.
    L4_Set_ExceptionHandler( vcpu.monitor_gtid );

    L4_ThreadId_t tid = L4_nilthread;
    L4_ThreadId_t ack_tid = L4_nilthread;
    L4_Clock_t last_time = {raw: 0}, current_time = {raw: 0};
    L4_Clock_t time_skew = {raw: 0};
    L4_Word_t timer_irq = INTLOGIC_TIMER_IRQ;
    L4_Time_t periodic;
    intlogic_t &intlogic = get_intlogic();
    i8253_counter_t *timer0 = &(i8253.counters[0]);
    word_t svector, sirq;
    
    last_time = L4_SystemClock();
    
    for (;;)
    {
	if(timer0->get_usecs() == 0) {
	    timer_length.raw = 54925;
	}
	else {
	    timer_length.raw = timer0->get_usecs();
	}

	if( time_skew > timer_length)
	    periodic = L4_TimePeriod( 1 );
	else
	periodic = L4_TimePeriod( timer_length.raw - time_skew.raw);

	L4_MsgTag_t tag = L4_ReplyWait_Timeout( ack_tid, periodic, &tid );

	ack_tid = L4_nilthread;
	
	if( L4_IpcFailed(tag) )
	{
	    L4_Word_t err = L4_ErrorCode();

	    if( (err & 0xf) == 3 ) { // Receive timeout.
		// Timer interrupt.

	    }
	    else if( (err & 0xf) == 2 ) { // Send timeout.
		//printf( "$"); // "IPC send timeout for IRQ delivery to main thread.\n");
		intlogic.reraise_vector(svector, sirq);
		continue;
	    }
	    else {
		DEBUGGER_ENTER("IRQ IPC failure");
		continue;
	    }
	} /* IPC timeout */

	// Received message.
	// should it not be: tid < system_base ?? --mb
	else if( tid.global.X.thread_no < tid_user_base ) {
	    // Hardware IRQ.
	    L4_Word_t irq = tid.global.X.thread_no;
	    dprintf(irq_dbg_level(irq), "hardware irq: %d int flag %d\n", irq, get_cpu().interrupts_enabled());
#if defined(CONFIG_DEVICE_PASSTHRU)
	    intlogic.set_hwirq_mask(irq);
	    intlogic.raise_irq( irq );
#else
	    DEBUGGER_ENTER("hardware irq");
#endif
	}
	else 
	{
	    // Virtual interrupt from external source.
	    if( msg_is_virq(tag) ) {
		L4_Word_t irq;
		msg_virq_extract( &irq );
		dprintf(irq_dbg_level(irq), "virtual irq: %d from TID %t\n", irq, tid);
		intlogic.raise_irq( irq );
	    }
	    else if( msg_is_hwirq_ack(tag) ) {
		DEBUGGER_ENTER("hwirq ack");

		L4_Word_t irq;
		msg_hwirq_ack_extract( &irq );
		dprintf(irq_dbg_level(irq), "hardware irq ack: %d int flag %d\n", irq, get_cpu().interrupts_enabled());
#if defined(CONFIG_DEVICE_PASSTHRU)
		// Send an ack message to the L4 interrupt thread.
		// TODO: the main thread should be able to do this via
		// propagated IPC.
		ack_tid.global.X.thread_no = irq;
		ack_tid.global.X.version = 1;
		L4_LoadMR( 0, 0 );  // Ack msg.
		continue;  // Don't attempt other interrupt processing.
#else
		DEBUGGER_ENTER("irq ack");
#endif
	    }
	    else if( msg_is_device_enable(tag) ) {
#if defined(CONFIG_DEVICE_PASSTHRU)
		L4_Word_t irq;
		L4_Error_t errcode;
		L4_Word_t prio = resourcemon_shared.prio + CONFIG_PRIO_DELTA_IRQ;
		dprintf(irq_dbg_level(irq), "enable device irq: %d\n", irq);

		msg_device_enable_extract(&irq);
		tid.global.X.thread_no = irq;
		tid.global.X.version = 1;
		errcode = AssociateInterrupt( tid, L4_Myself() );
		if( errcode != L4_ErrOk )
		    printf( "Attempt to associate an unavailable interrupt: %d L4 error: %s",
			    irq, L4_ErrString(errcode));
		if( !L4_Set_Priority(tid, prio) )
		    printf( "Unable to set irq %d priority %d, L4 errcode: %d\n",
			    irq, prio, L4_ErrorCode());
#else
		DEBUGGER_ENTER("device irq enable");
#endif
	    }
	    else
		printf( "unexpected IRQ message from %t\n", tid);
	}

	// Make sure that we deliver our timer interrupts too!
	current_time = L4_SystemClock();
	time_skew = time_skew + current_time - last_time;
	last_time = current_time;

	if(time_skew >= timer_length) {
	    time_skew = time_skew - timer_length;
	    dprintf(irq_dbg_level(timer_irq), ", timer irq %d if %x\n", timer_irq, get_cpu().interrupts_enabled());
	    intlogic.raise_irq( timer_irq );
	}

	if(time_skew.raw > 1000000) // 1s
	    DEBUGGER_ENTER("Massive time skew detected!");


	if( intlogic.pending_vector( svector, sirq ) ) 
	{
	    // notify monitor thread
	    msg_vector_build( svector, sirq);
	    ack_tid = vcpu.monitor_ltid;
	    
	}
    } /* while */
}

#if defined(CONFIG_DEVICE_APIC)
/**
 * this is a major guess fest; L4 doesn't export the APIC IRQ sources
 * so we have to guess what they could have been 
 */

static void init_io_apics()
{
    int sources = L4_ThreadIdSystemBase(L4_GetKernelInterface());
    int apic;
    int nr_ioapics = acpi.get_nr_ioapics();
    intlogic_t &intlogic = get_intlogic();

    if (!nr_ioapics) {
	printf( "IOAPIC Initialization of APICs not possible, ignore...\n");
	DEBUGGER_ENTER("IOAPIC initialization failed");
	return;
    }
    
    if (nr_ioapics >= CONFIG_MAX_IOAPICS)
    {
	printf( "IOAPIC more real IOAPICs than virtual APICs\n");
	panic();
    }
    
    printf( "IOAPIC found %d sources on %d apics\n", souces, nr_ioapics);

   
    for (apic=0; apic < nr_ioapics; apic++)
    {
	intlogic.ioapic[apic].set_id(acpi.get_ioapic_id(apic));
	intlogic.ioapic[apic].set_base(acpi.get_ioapic_irq_base(apic));
    }
    
    /* 
     * This is an incredible hack: we _guess_ the real IO-APIC's 
     * redirection entries
     */
    
    // first the very simple version--identical APIC wire numbers
    if ((sources / nr_ioapics) == 16 || 
	(sources / nr_ioapics) == 24 || 
	(sources / nr_ioapics) == 44) 
    {
	for (apic = 0; apic < nr_ioapics; apic++)
	    intlogic.ioapic[apic].set_max_redir_entry(sources / nr_ioapics - 1);
    } 
    else {
	switch (nr_ioapics) {
	case 3: // as seen on Opterons
	    if ((sources - 16) == 8) {
		intlogic.ioapic[0].set_max_redir_entry(15);
		intlogic.ioapic[1].set_max_redir_entry(3);
		intlogic.ioapic[2].set_max_redir_entry(3);
		break;
	    }
	    if ((sources - 24) == 8) {
		intlogic.ioapic[0].set_max_redir_entry(23);
		intlogic.ioapic[1].set_max_redir_entry(3);
		intlogic.ioapic[2].set_max_redir_entry(3);
		break;
	    }
	default:
	    printf( "IOAPIC unknown HW IOAPIC configuration (%d IOAPICs, %d IRQ sourced\n",
		    nr_ioapics, sources);
	}
    }
   
    
    // mark all remaining IO-APICs as invalid
    for (apic = nr_ioapics; apic < CONFIG_MAX_IOAPICS; apic++)
	intlogic.ioapic[apic].set_id(0xf);

    // pin/hwirq to IO-APIC association
    for (word_t i = 0; i < CONFIG_MAX_IOAPICS; i++)
    {
	printf( "IOAPIC id %d\n", intlogic.ioapic[i].get_id());
	if (!intlogic.ioapic[i].is_valid_ioapic())
	    continue;

	word_t hwirq_min = intlogic.ioapic[i].get_base();
	word_t hwirq_max = hwirq_min + intlogic.ioapic[i].get_max_redir_entry() + 1;
	hwirq_max = hwirq_max >= INTLOGIC_MAX_HWIRQS ? INTLOGIC_MAX_HWIRQS: hwirq_max;
	    
	for (word_t hwirq = hwirq_min; hwirq < hwirq_max; hwirq++)
	{
	    printf( "IOAPIC registering hwirq %d with apic %d\n", 
		    hwirq, intlogic.ioapic[i].get_id());
	    intlogic.hwirq_register_ioapic(hwirq, &intlogic.ioapic[i]);
	}
    }
	
}
    

#endif

L4_ThreadId_t irq_init( L4_Word_t prio, L4_ThreadId_t pager_tid, vcpu_t *vcpu )
{
    hthread_t *irq_thread =
	get_hthread_manager()->create_thread( 
	    &get_vcpu(), (L4_Word_t)irq_stack, sizeof(irq_stack),
		prio, irq_handler_thread, pager_tid, vcpu );

    if( !irq_thread )
	return L4_nilthread;

    irq_thread->start();
#if defined(CONFIG_DEVICE_APIC)
    init_io_apics();
#endif

    return irq_thread->get_local_tid();
}

