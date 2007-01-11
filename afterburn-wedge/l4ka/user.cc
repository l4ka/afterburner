/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/user.cc
 * Description:   House keeping code for mapping L4 threads to guest threads.
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
 * $Id: user.cc,v 1.8 2005/04/13 14:35:54 joshua Exp $
 *
 ********************************************************************/
#include <bind.h>
#include <l4/kip.h>
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/kip.h>

#include INC_ARCH(bitops.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(memory.h)
#include INC_WEDGE(user.h)
#include INC_WEDGE(hthread.h)
#include INC_WEDGE(l4privileged.h)

static const bool debug_user_pfault=0;
static const bool debug_thread_allocate=0;
static const bool debug_thread_exit=0;

thread_manager_t thread_manager;
task_manager_t task_manager;
cpu_lock_t thread_mgmt_lock;

L4_Word_t task_info_t::utcb_size = 0;
L4_Word_t task_info_t::utcb_base = 0;

void task_info_t::init()
{
    for( L4_Word_t i = 0; i < sizeof(utcb_mask)/sizeof(utcb_mask[0]); i++ )
	utcb_mask[i] = 0;
    
    if( 0 == task_info_t::utcb_size ) {
	task_info_t::utcb_size = L4_UtcbSize( L4_GetKernelInterface() );
	task_info_t::utcb_base = get_vcpu().get_kernel_vaddr();
    }
}

thread_info_t::thread_info_t()
{
    tid = L4_nilthread;
}

task_info_t *
task_manager_t::find_by_page_dir( L4_Word_t page_dir )
{
    task_info_t *ret = 0;
    L4_Word_t idx_start = hash_page_dir( page_dir );

    L4_Word_t idx = idx_start;
    do {
	if( tasks[idx].page_dir == page_dir )
	{
	    ret =  &tasks[idx];
	    goto done;
	}
	idx = (idx + 1) % max_tasks;
    } while( idx != idx_start );

    // TODO: go to an external process for dynamic memory.
 done:
    return ret;
}

task_info_t *
task_manager_t::allocate( L4_Word_t page_dir )
{
    task_info_t *ret = NULL;
    
    L4_Word_t idx_start = hash_page_dir( page_dir );

    L4_Word_t idx = idx_start;
    do {
	if( tasks[idx].page_dir == 0 ) {
	    tasks[idx].page_dir = page_dir;
	    ret =  &tasks[idx];
	    goto done;
	}
	idx = (idx + 1) % max_tasks;
    } while( idx != idx_start );


 done:
    // TODO: go to an external process for dynamic memory.
    return ret;
}

void 
task_manager_t::deallocate( task_info_t *ti )
{ 
    //ptab_info.clear(ti->page_dir);
    ti->page_dir = 0; 
}

thread_info_t *
thread_manager_t::find_by_tid( L4_ThreadId_t tid )
{
    L4_Word_t start_idx = hash_tid( tid );

    L4_Word_t idx = start_idx;
    do {
	if( threads[idx].tid == tid )
	    return &threads[idx];
	idx = (idx + 1) % max_threads;
    } while( idx != start_idx );

    // TODO: go to an external process for dynamic memory.
    return 0;
}

thread_info_t *
thread_manager_t::allocate( L4_ThreadId_t tid )
{
    thread_info_t *ret = NULL;
    
    L4_Word_t start_idx = hash_tid( tid );
    L4_Word_t idx = start_idx;
    do {
	if( L4_IsNilThread(threads[idx].tid) ) {
	    threads[idx].tid = tid;
	    ret = &threads[idx];
	    goto done;
	}
	idx = (idx + 1) % max_threads;
    } while( idx != start_idx );

 done: 
    // TODO: go to an external process for dynamic memory.
    return ret;
}

bool handle_user_pagefault( vcpu_t &vcpu, thread_info_t *thread_info, L4_ThreadId_t tid,  L4_MapItem_t &map_item )
// When entering and exiting, interrupts must be disabled
// to protect the message registers from preemption.
{
    word_t map_addr, map_bits, map_rwx;
    map_item = L4_MapItem(L4_Nilpage, 0);
    
    ASSERT( !vcpu.cpu.interrupts_enabled() );

    // Extract the fault info.
    L4_Word_t fault_rwx = thread_info->mr_save.get_pfault_rwx();
    L4_Word_t fault_addr = thread_info->mr_save.get_pfault_addr();
    L4_Word_t fault_ip = thread_info->mr_save.get_pfault_ip();

    if( debug_user_pfault )
	con << "User fault from TID " << tid
	    << ", addr " << (void *)fault_addr
	    << ", ip " << (void *)fault_ip
	    << ", rwx " << fault_rwx << '\n';
    
    if (fault_addr == 0)
    {
    	L4_KDB_Enter("PFBUG1");
    	con << "User fault from TID " << tid
	    << ", addr " << (void *)fault_addr
	    << ", ip " << (void *)fault_ip
	    << ", rwx " << fault_rwx 
	    << '\n';
	for (word_t id=0; id < CONFIG_NR_VCPUS; id++)
	{
	    thread_info_t * vcpu_info = thread_info->ti->get_vcpu_thread(id);
	    con << id << " " << vcpu_info->get_tid() << "\n";
	}
	L4_KDB_Enter("PFBUG1");

    }


#if defined(CONFIG_VSMP)
    if (EXPECT_FALSE(is_helper_addr(fault_addr)))
    {
	map_addr = fault_addr;
	map_rwx = 5;
	map_bits = PAGE_BITS;
	if (debug_helper)
	{
	    con << "Helper pfault " 
		<< ", addr " << (void *) fault_addr 
		<< ", TID " << thread_info->get_tid()
		<< "\n";
	}
	goto done;
    }
#endif    
    thread_mgmt_lock.lock("tmgmt");
    word_t page_dir = thread_info->ti->get_page_dir();
    thread_mgmt_lock.unlock();
    
    // Lookup the translation, and handle the fault if necessary.
    bool complete = backend_handle_user_pagefault( 
	page_dir, fault_addr, fault_ip, fault_rwx,
	map_addr, map_bits, map_rwx, thread_info );
    if( !complete )
	return false;

    ASSERT( !vcpu.cpu.interrupts_enabled() );
    
#if defined(CONFIG_VSMP)
 done:
#endif
    // Build the reply message to user.
    map_item = L4_MapItem(
	L4_FpageAddRights(L4_FpageLog2(map_addr, map_bits),  map_rwx), 
	fault_addr );
    
    if( debug_user_pfault )
	con << "Page fault reply to TID " << tid
	    << ", kernel addr " << (void *)map_addr
	    << ", size " << (1 << map_bits)
	    << ", rwx " << map_rwx
	    << ", user addr " << (void *)fault_addr << '\n';

    
    return true;
}


thread_info_t *allocate_user_thread(task_info_t *task_info)
{
    L4_Error_t errcode;
    vcpu_t &vcpu = get_vcpu();
    L4_ThreadId_t controller_tid = vcpu.main_gtid;
   
    // Allocate a thread ID.
    L4_ThreadId_t tid = get_hthread_manager()->thread_id_allocate();
    if( L4_IsNilThread(tid) )
	PANIC( "Out of thread IDs." );

    if (!task_info)
    {
	// Lookup the address space's management structure.
	L4_Word_t page_dir = vcpu.cpu.cr3.get_pdir_addr();
	task_info = 
	    task_manager_t::get_task_manager().find_by_page_dir( page_dir );
	
	if( !task_info )
	{
	    // New address space.
	    task_info = task_manager_t::get_task_manager().allocate( page_dir );
	    if( !task_info )
		PANIC( "Hit task limit." );
	    task_info->init();
	}
    }

    // Choose a UTCB in the address space.
    L4_Word_t utcb, utcb_index;
    if( !task_info->utcb_allocate(utcb, utcb_index) )
	PANIC( "Hit task thread limit." );

    // Configure the TID.
    tid = L4_GlobalId( L4_ThreadNo(tid), task_info_t::encode_gtid_version(utcb_index) );
    if( task_info_t::decode_gtid_version(tid) != utcb_index )
	PANIC( "L4 thread version wrap-around." );
    
    // Allocate a thread info structure.
    thread_info_t *thread_info = 
	thread_manager_t::get_thread_manager().allocate( tid );
    if( !thread_info )
	PANIC( "Hit thread limit." );
    
    thread_info->init();
    thread_info->ti = task_info;

    // Init the L4 address space if necessary.
    if( task_info->utcb_count() == 1 )
    {
	task_info->set_space_tid(tid);
	// Create the L4 thread.
	errcode = ThreadControl( tid, tid, controller_tid, L4_nilthread, utcb );
	if( errcode != L4_ErrOk )
	    PANIC( "Failed to create initial user thread, TID " << tid 
		    << ", L4 error: " << L4_ErrString(errcode) );
	
	// Create an L4 address space + thread.
	// TODO: don't hardcode the size of a utcb to 512-bytes
	task_info->set_utcb_fp(L4_Fpage( user_vaddr_end, 512*CONFIG_L4_MAX_THREADS_PER_TASK ));
	task_info->set_kip_fp(L4_Fpage( L4_Address(task_info->get_utcb_fp()) + L4_Size(task_info->get_utcb_fp()), KB(16)));
	errcode = SpaceControl( tid, 0, task_info->get_kip_fp(), task_info->get_utcb_fp(), L4_nilthread );
	if( errcode != L4_ErrOk )
	    PANIC( "Failed to create an address space, TID " << tid 
		    << ", L4 error: " << L4_ErrString(errcode) );
	
	
    }
    
    ASSERT(task_info->has_space_tid());
    
    // Create the L4 thread.
    errcode = ThreadControl( tid, task_info->get_space_tid(), controller_tid, controller_tid, utcb );
	
    if( errcode != L4_ErrOk )
	PANIC( "Failed to create user thread, TID " << tid 
		<< ", space TID " << task_info->get_space_tid()
		<< ", utcb " << (void *)utcb 
		<< ", L4 error: " << L4_ErrString(errcode) );

    
    
    L4_Word_t dummy;
#if defined(CONFIG_L4KA_VMEXTENSIONS)
    // Set the thread's exception handler via exregs
    L4_Msg_t msg;
    L4_ThreadId_t local_tid, dummy_tid;
    L4_MsgClear( &msg );
    L4_MsgAppendWord (&msg, controller_tid.raw);
    L4_MsgLoad( &msg );
    local_tid = L4_ExchangeRegisters( tid, (1 << 9), 
	    0, 0, 0, 0, L4_nilthread, 
	    &dummy, &dummy, &dummy, &dummy, &dummy,
	    &dummy_tid );
    if( L4_IsNilThread(local_tid) ) {
	PANIC("Failed to set user thread's exception handler\n");
    }
    
    L4_Word_t preemption_control = L4_PREEMPTION_CONTROL_MSG;
    L4_Word_t time_control = (L4_Never.raw << 16) | L4_Never.raw;
#else
    L4_Word_t preemption_control = ~0UL;
    L4_Word_t time_control = ~0UL;
#endif    
    // Set the thread priority.
    L4_Word_t prio = vcpu.get_vcpu_max_prio() + CONFIG_PRIO_DELTA_USER;    
    L4_Word_t processor_control = vcpu.pcpu_id & 0xffff;
    
    if (!L4_Schedule(tid, time_control, processor_control, prio, preemption_control, &dummy))
	PANIC( "Failed to either enable preemption msgs"
		<<" or to set user thread's priority to " << prio 
		<< " or to set user thread's processor number to " << vcpu.pcpu_id
		<<" or to set user thread's timeslice/quantum to " << (void *) time_control );


    if (debug_thread_allocate)
	con << "create user thread" 
	    << ", TID " << tid
	    << ", space TID " << task_info->get_space_tid()
	    << ", utcb " << (void *)utcb  
	    << "\n";
    
    thread_info->vcpu_id = vcpu.cpu_id;
    thread_info->state = thread_state_startup;
    

    return thread_info;
}

void delete_user_thread( thread_info_t *thread_info )
{
    if( !thread_info )
	return;
   
    L4_ThreadId_t tid = thread_info->get_tid();
    task_info_t *task_info = thread_info->ti;
    L4_Word_t utcb_count = task_info->utcb_count();

    for (word_t id=0; id < CONFIG_NR_VCPUS; id++)
    {
	thread_info_t * vcpu_info = task_info->get_vcpu_thread(id);
	if (vcpu_info != thread_info && vcpu_info == get_vcpu(id).user_info)
	{
	    if (debug_thread_exit)
		con << "do not delete thread " << tid 
		    << ", sibling " << vcpu_info->get_tid() << "in use\n";
	    return;
	}
    }
	
    ASSERT( thread_info->ti );


    if( debug_thread_exit )
	con << "delete user thread" 
	    << ", TID " << tid
	    << ", space TID " << task_info->get_space_tid()
	    << ", utcb_count " << utcb_count
	    << "\n";

    if( task_info->get_space_tid() != tid || utcb_count <= CONFIG_NR_VCPUS )
    {
	/* Kill thread */
	ASSERT(tid != L4_nilthread);
	ThreadControl( tid, L4_nilthread, L4_nilthread, L4_nilthread, ~0UL );
#if defined(CONFIG_VSMP)
	task_info->set_vcpu_thread(thread_info->vcpu_id, NULL);
#endif
	get_hthread_manager()->thread_id_release( tid );
	thread_manager_t::get_thread_manager().deallocate( thread_info );
	task_info->utcb_release( task_info_t::decode_gtid_version(tid) );
	
	if (utcb_count <= CONFIG_NR_VCPUS)	
	{
#if defined(CONFIG_VSMP)
	    /* Kill all VCPU siblings */
	    for (word_t id=0; id < CONFIG_NR_VCPUS; id++)
	    {
		thread_info_t * vcpu_info = task_info->get_vcpu_thread(id);
		if (vcpu_info && vcpu_info != thread_info && vcpu_info != get_vcpu(id).user_info)
		{
		    if( debug_thread_exit )
			con << "delete sibling thread" 
			    << ", TID " << vcpu_info->get_tid()
			    << "\n";
		    ASSERT(vcpu_info->get_tid() != L4_nilthread);
		    ThreadControl( vcpu_info->get_tid(), L4_nilthread, L4_nilthread, L4_nilthread, ~0UL );
		    get_hthread_manager()->thread_id_release( vcpu_info->get_tid() );
		    task_info->set_vcpu_thread(id, NULL);
		    thread_manager_t::get_thread_manager().deallocate( vcpu_info );
		    task_info->utcb_release( task_info_t::decode_gtid_version(vcpu_info->get_tid()) );
		}
	    }
#endif
	    task_manager_t::get_task_manager().deallocate( task_info );
	}
    }
    else
    {
	// Keep the space thread alive, until the address space is empty.
	// Just flip a flag to say that the space thread is invalid.
	task_info->invalidate_space_tid();
    }
    

}



#if defined(CONFIG_VSMP)
extern word_t afterburner_helper[];
word_t afterburner_helper_addr = (word_t) afterburner_helper;
extern word_t afterburner_helper_done[];
word_t afterburner_helper_done_addr = (word_t) afterburner_helper_done;

__asm__ ("						\n\
	.section .text.user, \"ax\"			\n\
	.balign	4096					\n\
afterburner_helper:					\n\
	1:						\n\
	movl	%ebx, -52(%edi)				\n\
	movl	%eax, -28(%edi)				\n\
	andl	$0x7fffffff, %eax			\n\
	movl	%edx, 4(%edi)				\n\
	movl	%esp, 8(%edi)				\n\
	movl	%ecx, 12(%edi)				\n\
	leal    -12(%edi), %esp				\n\
	call	*%ebp					\n\
	movl	-28(%edi), %eax				\n\
	testl   $0x80000000, %eax			\n\
	jnz	2f					\n\
	andl	$0x7fffffff, %eax		        \n\
	andl	$0x3f, %eax				\n\
	addl	$1, %eax				\n\
	movl	$0x4, %ebx				\n\
	cmpl    %ebx, %eax				\n\
	cmovle	%ebx, %eax				\n\
	leal	4(%edi,%eax,4), %eax			\n\
	movl	 (%eax), %ebx				\n\
	subl	 %edi, %ebx				\n\
	subl	 $5, %ebx				\n\
	movb     $0xe9, (%edi)				\n\
	movl     %ebx,  1(%edi)				\n\
	leal	4(%eax), %esp				\n\
	popfl	 		 			\n\
	movl	 8(%eax), %edi				\n\
	movl	12(%eax), %esi				\n\
	movl	16(%eax), %ebp				\n\
	movl	20(%eax), %esp				\n\
	movl	24(%eax), %ebx				\n\
	movl	28(%eax), %edx				\n\
	movl	32(%eax), %ecx				\n\
	movl 	36(%eax), %eax				\n\
	jmpl   *%gs:0	  				\n\
	2:						\n\
        movl   $0x12,-64(%edi)				\n\
        movl   -48(%edi), %eax				\n\
        movl   %eax, %edx				\n\
        xorl   %ecx, %ecx				\n\
        movl   $0xffd10000, %esi			\n\
	leal    -12(%edi), %esp				\n\
	jmpl   *-52(%edi)				\n\
afterburner_helper_done:				\n\
	.previous					\n\
");


L4_Word_t task_info_t::commit_helper(bool piggybacked=false)
{
    
    thread_mgmt_lock.lock("tmgmt");
    
    if (unmap_count == 0)
    {
	thread_mgmt_lock.unlock();
	return 0;
    }
    
#if 0
    if (piggybacked)
	L4_KDB_PrintChar('*');
    else
	L4_KDB_PrintChar('+');
#endif
    
    vcpu_t vcpu = get_vcpu();
    thread_info_t *vcpu_info = vcpu_thread[vcpu.cpu_id]; 

    if (vcpu_info == NULL)
    {
	vcpu_info = allocate_user_thread(this);
	vcpu_thread[vcpu.cpu_id] = vcpu_info;
	vcpu_info->state = thread_state_preemption;
	vcpu_info->mr_save.set_msg_tag( (L4_MsgTag_t) { raw : 0xffd10000 } );
    }
 
    L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *)
        L4_GetKernelInterface();

   
    L4_Word_t utcb_index = decode_gtid_version(vcpu_info->get_tid());
    L4_Word_t utcb = utcb_index*utcb_size + utcb_base + 0x100;
   
    if (debug_helper)
	con << "helper "
	    << (piggybacked ? " piggybacked " : "not piggybacked")
	    << " unmap " << unmap_count
	    << " vcpu_info " << vcpu_info
	    << " tid " << vcpu_info->get_tid()
	    << " utcb " << (void *) utcb
	    << "\n";


    /* Dummy MRs1..3, since the preemption logic will restore the old ones */
    L4_Word_t untyped = 3;
    L4_Word_t dummy_mr[3];
    L4_LoadMRs( 1, untyped, (L4_Word_t *) &dummy_mr);	
	
    /* Unmap pages 4 .. n */
    if (unmap_count > 4)
    {
	L4_LoadMRs( 1 + untyped, unmap_count-3, (L4_Word_t *) &unmap_pages[4]);	
	untyped += unmap_count-4;
    }

    if (piggybacked) 
    {
	/* Old ctrlxfer item, will be set by helper */
	L4_CtrlXferItem_t old_ctrlxfer;
	for (word_t i=0; i < CTRLXFER_SIZE; i++)
	    old_ctrlxfer.raw[i] = vcpu_info->mr_save.get(3+i);

		
	if (debug_helper)
	{
	    con << "orig   "
		<< ", eip " << (void *) old_ctrlxfer.eip	
		<< ", efl " << (void *) old_ctrlxfer.eflags
		<< ", edi " << (void *) old_ctrlxfer.edi
		<< ", esi " << (void *) old_ctrlxfer.esi
		<< ", ebp " << (void *) old_ctrlxfer.ebp
		<< ", esp " << (void *) old_ctrlxfer.esp
		<< ", ebx " << (void *) old_ctrlxfer.ebx
		<< ", edx " << (void *) old_ctrlxfer.edx
		<< ", ecx " << (void *) old_ctrlxfer.ecx
		<< ", eax " << (void *) old_ctrlxfer.eax
		<< "\n";
	}

	
	L4_SetCtrlXferMask(&old_ctrlxfer, 0x3ff);
	L4_LoadMRs( 1 + untyped, CTRLXFER_SIZE, old_ctrlxfer.raw);
	untyped += CTRLXFER_SIZE;	

	/* New ctrlxfer item */
	vcpu_info->mr_save.set(OFS_MR_SAVE_EIP, (word_t) afterburner_helper);
	vcpu_info->mr_save.set(OFS_MR_SAVE_EFLAGS, 0x3202);
	vcpu_info->mr_save.set(OFS_MR_SAVE_EDI, utcb);
	vcpu_info->mr_save.set(OFS_MR_SAVE_ESI, unmap_pages[0].raw);
	vcpu_info->mr_save.set(OFS_MR_SAVE_EDX, unmap_pages[1].raw);	
	vcpu_info->mr_save.set(OFS_MR_SAVE_ESP, unmap_pages[2].raw );
	vcpu_info->mr_save.set(OFS_MR_SAVE_ECX, unmap_pages[3].raw );
	vcpu_info->mr_save.set(OFS_MR_SAVE_EAX, 0x3f + unmap_count);
	vcpu_info->mr_save.set(OFS_MR_SAVE_EBX, L4_Address(kip_fp)+ kip->Ipc);	
	vcpu_info->mr_save.set(OFS_MR_SAVE_EBP, L4_Address(kip_fp)+ kip->Unmap);
	
	unmap_count = 0;
	thread_mgmt_lock.unlock();
	return untyped;

    }
    
    L4_MsgTag_t tag = L4_Niltag; 
    L4_CtrlXferItem_t ctrlxfer;
    ctrlxfer.eip = (word_t) afterburner_helper;
    ctrlxfer.eflags = 0x3202;
    ctrlxfer.edi = utcb;
    ctrlxfer.esi = unmap_pages[0].raw;
    ctrlxfer.edx = unmap_pages[1].raw;	
    ctrlxfer.esp = unmap_pages[2].raw ;
    ctrlxfer.ecx = unmap_pages[3].raw;
    ctrlxfer.eax = 0x8000003f + unmap_count;
    ctrlxfer.ebx = L4_Address(kip_fp)+ kip->Ipc;	
    ctrlxfer.ebp = L4_Address(kip_fp)+ kip->Unmap;
    L4_SetCtrlXferMask(&ctrlxfer, 0x3ff);
    L4_LoadMRs( 1 + untyped, CTRLXFER_SIZE, ctrlxfer.raw);
    tag.X.u = untyped;
    tag.X.t = CTRLXFER_SIZE;
    
    /*
     *  We go into dispatch mode; if the helper should be preempted,
     *  we'll get scheduled by the IRQ thread.
     */
    L4_ThreadId_t vcpu_tid = get_vcpu_thread(vcpu.cpu_id)->get_tid();
    
    unmap_count = 0;
    
    for (;;)
    {	
	L4_Set_MsgTag(tag);
	vcpu.dispatch_ipc_enter();
	
	tag = L4_Call(vcpu_tid);
	vcpu.dispatch_ipc_exit();
	
	switch (L4_Label(tag))
	{
	    case msg_label_pfault_start ... msg_label_pfault_end:
	    {
		L4_Word_t addr;
		L4_StoreMR(OFS_MR_SAVE_PF_ADDR, &addr);
		ASSERT(is_helper_addr(addr));
		if (debug_helper)
		    con << "helper pf " << (void *) addr << "\n";
		L4_MapItem_t map_item = L4_MapItem(
		    L4_FpageAddRights(L4_FpageLog2(addr, PAGE_BITS), 5), addr );
		tag = L4_Niltag;
		tag.X.t = 2;
		L4_LoadMRs(1, 2, (L4_Word_t *) &map_item);
		break;
		
	    }
	    case msg_label_preemption:
	    {
		if (debug_helper)
		    con << "helper restart " << vcpu_info->get_tid() << "\n";
		tag = L4_Niltag;
		break;	
		
	    }
	    case msg_label_preemption_yield:
	    {
		if (debug_helper)
		    con << "helper done " << vcpu_info->get_tid() << "\n";
		thread_mgmt_lock.unlock();
		return 0;		
	    }
	    default:
	    {
		
		con << "VMEXT Bug invalid helper thread state\n";
		DEBUGGER_ENTER();
		
	    }
	    break;
	}
    } 
    thread_mgmt_lock.unlock();
}

#endif /* defined(CONFIG_VSMP) */

		 
	

    
