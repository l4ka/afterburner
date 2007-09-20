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

static const bool debug_task_allocate=0;
static const bool debug_task_exit=0;

thread_manager_t thread_manager;
task_manager_t task_manager;
cpu_lock_t thread_mgmt_lock;

L4_Word_t task_info_t::utcb_area_size = 0;
L4_Word_t task_info_t::utcb_size = 0;
L4_Word_t task_info_t::utcb_base = 0;

#if defined(CONFIG_VSMP)
extern word_t afterburner_helper_addr;
#endif
 
static L4_KernelInterfacePage_t *kip;

void task_info_t::init()
{
    if (!kip)
	kip = (L4_KernelInterfacePage_t *) L4_GetKernelInterface();
    utcb_size = L4_UtcbSize(kip);
    
    if (utcb_area_size == 0)
    {
	if (vcpu_t::nr_vcpus * L4_UtcbSize(kip) > L4_UtcbAreaSize( kip ))
	    utcb_area_size = vcpu_t::nr_vcpus;
	else
	    utcb_area_size = L4_UtcbAreaSize( kip );
	
	utcb_base = get_vcpu().get_kernel_vaddr();
    }
    
    for (word_t id=0; id<vcpu_t::nr_vcpus; id++)
	vcpu_thread[id] = NULL;
    unmap_count = 0;
    vcpu_ref_count = 0;
    vcpu_thread_count = 0;
    freed = false;
}

task_info_t *
task_manager_t::find_by_page_dir( L4_Word_t page_dir, bool alloc )
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
    
    if (!ret && alloc)
    {
	ret = allocate( page_dir );
	ret->init();
    }

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
    ti->unmap_count = 0;
    ti->freed = false;
    ti->vcpu_ref_count = 0;
    ti->vcpu_thread_count = 0;
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

static word_t l4_threadcount;

thread_info_t *task_info_t::allocate_vcpu_thread()
{

    L4_Error_t errcode;
    vcpu_t &vcpu = get_vcpu();
    L4_ThreadId_t controller_tid = vcpu.main_gtid;
   

    // Allocate a thread ID.
    L4_ThreadId_t tid = get_hthread_manager()->thread_id_allocate();
    ASSERT(!L4_IsNilThread(tid));
    
    L4_Word_t utcb = utcb_base + (vcpu.cpu_id * task_info_t::utcb_size);
    
    // Allocate a thread info structure.
    ASSERT(!vcpu_thread[vcpu.cpu_id]);
    vcpu_thread[vcpu.cpu_id] = get_thread_manager().allocate( tid );
    ASSERT( vcpu_thread[vcpu.cpu_id] );


    vcpu_thread[vcpu.cpu_id]->init();
    vcpu_thread[vcpu.cpu_id]->ti = this;

    	
    // Init the L4 address space if necessary.
    if( ++vcpu_thread_count == 1 )
    {
	// Create the L4 thread.
	errcode = ThreadControl( tid, tid, controller_tid, L4_nilthread, utcb );
	if( errcode != L4_ErrOk )
	    PANIC( "Failed to create initial user thread, TID %t, L4 error %s", 
		   tid,  L4_ErrString(errcode) );
	
	ASSERT(kip);
	utcb_fp = L4_Fpage( utcb_base, utcb_area_size);
	kip_fp = L4_Fpage( L4_Address(utcb_fp) + L4_Size(utcb_fp), L4_KipAreaSize(kip));
	
	errcode = SpaceControl( tid, 0, kip_fp, utcb_fp, L4_nilthread );
	if ( errcode != L4_ErrOk )
	    PANIC( "Failed to create address space, TID %t, L4 error %s", 
		   tid,  L4_ErrString(errcode) );
	
	space_tid = tid;	
    }
    
    ASSERT(space_tid != L4_nilthread);

    if (debug_task_allocate)
    {
	con << "alloc"  
	    << ", TID " << tid
	    << ", ti " << vcpu_thread[vcpu.cpu_id]
	    << ", space TID " << space_tid
	    << ", utcb " << (void *)utcb  
	    << "\n";
    }

    // Create the L4 thread.
    errcode = ThreadControl( tid, space_tid, controller_tid, controller_tid, utcb );
    ++l4_threadcount;
    
    if( errcode != L4_ErrOk )
	    PANIC( "Failed to create user thread, TID %t, utcb %x, L4 error %s", 
		   tid, utcb, L4_ErrString(errcode) );
    
    L4_Word_t dummy;
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
    // Set the thread priority.
    L4_Word_t prio = vcpu.get_vcpu_max_prio() + CONFIG_PRIO_DELTA_USER;    
    L4_Word_t processor_control = vcpu.get_pcpu_id() & 0xffff;
    
    if (!L4_Schedule(tid, time_control, processor_control, prio, preemption_control, &dummy))
	PANIC( "Failed to either enable preemption msgs "
	       "or to set user thread's priority to %d "
	       "or to set user thread's processor number to %d "
	       "or to set user thread's timeslice/quantum to %x\n",
	       prio, vcpu.get_pcpu_id(), time_control);
    

    //con << l4_threadcount << "+\n";
    vcpu_thread[vcpu.cpu_id]->state = thread_state_startup;
 
   
    return vcpu_thread[vcpu.cpu_id];
}

void task_info_t::free( )
{

    L4_Error_t errcode;
 
    ASSERT(vcpu_ref_count == 0);
    
    if(debug_task_exit)
    {
	con << "delete task " << (void *) this
	    << ", space TID " << space_tid
	    << ", count " << vcpu_thread_count
	    << "\n";
    }
    
    for (word_t id=0; id < vcpu_t::nr_vcpus; id++)
	ASSERT (get_vcpu(id).cpu_id == id || vcpu_thread[id] != get_vcpu(id).user_info);
    
    for (word_t id=0; id < vcpu_t::nr_vcpus; id++)
    {
	if (vcpu_thread[id])
	{
	    L4_ThreadId_t tid = vcpu_thread[id]->get_tid();
	    ASSERT(tid != L4_nilthread);

	    --vcpu_thread_count;

	    --l4_threadcount;
	    
	    /* Kill thread */
	    errcode = ThreadControl( tid, L4_nilthread, L4_nilthread, L4_nilthread, ~0UL );
	    ASSERT(errcode  == L4_ErrOk);
	    get_hthread_manager()->thread_id_release( tid );
	    get_thread_manager().deallocate( vcpu_thread[id] );
	    vcpu_thread[id] = NULL;

	}
    }
    
    //con << l4_threadcount << "-\n";
    ASSERT(vcpu_thread_count == 0);
    space_tid = L4_nilthread;
    get_task_manager().deallocate( this );

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
	int	$0x3		    /* XXX */		\n\
	2:						\n\
        movl   $0x12,-64(%edi)				\n\
        movl   -48(%edi), %eax				\n\
        movl   %eax, %edx				\n\
        xorl   %ecx, %ecx				\n\
	leal    -12(%edi), %esp				\n\
	jmpl   *-52(%edi)				\n\
afterburner_helper_done:				\n\
	.previous					\n\
");


L4_Word_t task_info_t::commit_helper()
{
   
    if (unmap_count == 0)
	return 0;

    //con << "*";

    vcpu_t vcpu = get_vcpu();
    thread_info_t *vcpu_info = vcpu_thread[vcpu.cpu_id]; 

    if (vcpu_info == NULL)
    {
	vcpu_info = allocate_vcpu_thread();
	ASSERT(vcpu_info);
	vcpu_info->state = thread_state_preemption;
	vcpu_info->mr_save.set_msg_tag( (L4_MsgTag_t) { raw : 0xffd10000 } );
    }
 
    L4_Word_t utcb = utcb_base + (vcpu.cpu_id * task_info_t::utcb_size) + 0x100;
   
    if (debug_helper)
	con << "ch " << this
	    << " vci " << vcpu_info
	    << " tid " << vcpu_info->get_tid()
	    << " ct " << unmap_count	
	    << "\n";


    /* Dummy MRs1..3, since the preemption logic will restore the old ones */
    L4_Word_t untyped = 3;
    L4_Word_t dummy_mr[3] = { 0, 0, 0 };
    L4_LoadMRs( 1, untyped, (L4_Word_t *) &dummy_mr);	
	
    /* Unmap pages 4 .. n */
    if (unmap_count > 4)
    {
	L4_LoadMRs( 1 + untyped, unmap_count-3, (L4_Word_t *) &unmap_pages[4]);	
	untyped += unmap_count-4;
    }

    L4_MsgTag_t tag = L4_Niltag; 
    L4_CtrlXferItem_t ctrlxfer;
    ctrlxfer.eip = (word_t) afterburner_helper;
    ctrlxfer.eflags = 0x3202;
    ctrlxfer.edi = utcb;
    ctrlxfer.esi = unmap_pages[0].raw;
    ctrlxfer.edx = unmap_pages[1].raw;
    ctrlxfer.esp = unmap_pages[2].raw;
    ctrlxfer.ecx = unmap_pages[3].raw;
    ctrlxfer.eax = 0x8000003f + unmap_count;
    ctrlxfer.ebx = L4_Address(kip_fp)+ kip->ThreadSwitch;	
    ctrlxfer.ebp = L4_Address(kip_fp)+ kip->Unmap;
    L4_SetCtrlXferMask(&ctrlxfer, 0x3ff, false);
    L4_LoadMRs( 1 + untyped, CTRLXFER_SIZE, ctrlxfer.raw);
    tag.X.u = untyped;
    tag.X.t = CTRLXFER_SIZE;
    
    /*
     *  We go into dispatch mode; if the helper should be preempted,
     *  we'll get scheduled by the IRQ thread.
     */
    L4_ThreadId_t vcpu_tid = get_vcpu_thread(vcpu.cpu_id)->get_tid();
    
    for (;;)
    {	
	L4_Set_MsgTag(tag);
	vcpu.dispatch_ipc_enter();
	L4_Accept(L4_UntypedWordsAcceptor);
	tag = L4_Call(vcpu_tid);
	ASSERT(!L4_IpcFailed(tag));
	vcpu.dispatch_ipc_exit();
	
	switch (L4_Label(tag))
	{
	    case msg_label_pfault_start ... msg_label_pfault_end:
	    {
		L4_Word_t addr;
		L4_StoreMR(OFS_MR_SAVE_PF_ADDR, &addr);
		ASSERT(addr >= afterburner_helper_addr && 
			addr <= afterburner_helper_done_addr);
		* ((volatile L4_Word_t *) addr);
		
		L4_MapItem_t map_item = L4_MapItem(
		    L4_FpageAddRights(L4_FpageLog2(addr, PAGE_BITS), 5), addr );
		tag = L4_Niltag;
		tag.X.t = 2;
		L4_LoadMRs(1, 2, (L4_Word_t *) &map_item);
		break;
		
	    }
	    case msg_label_preemption:
	    {
		tag = L4_Niltag;
		break;	
		
	    }
	    case msg_label_preemption_yield:
	    {
		unmap_count = 0;
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
}

#endif /* defined(CONFIG_VSMP) */

		 
	

    
