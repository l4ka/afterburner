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
#include <console.h>
#include <debug.h>

#include <l4/kip.h>
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/kip.h>

#include INC_ARCH(bitops.h)
#include INC_WEDGE(backend.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(memory.h)
#include INC_WEDGE(l4thread.h)
#include INC_WEDGE(l4privileged.h)


cpu_lock_t thread_mgmt_lock;

L4_Word_t task_info_t::utcb_area_size = 0;
L4_Word_t task_info_t::utcb_size = 0;
L4_Word_t task_info_t::utcb_area_base = 0;

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
	
	utcb_area_base = get_vcpu().get_kernel_vaddr();
    }
    
    for (word_t id=0; id<vcpu_t::nr_vcpus; id++)
	vcpu_thread[id] = NULL;
    
    unmap_count = 0;
    vcpu_ref_count = 0;
    vcpu_thread_count = 0;
    freed = false;
}



bool
thread_manager_t::resume_vm_threads()
{
    for (unsigned int i = 0; i < max_threads; i++)
    {
	if (threads[i].tid != L4_nilthread)
	{
	    printf( "resuming thread: %t ", threads[i].tid);
	    
	    // allocate new thread ID
	    L4_ThreadId_t tid = get_l4thread_manager()->thread_id_allocate();
	    if (L4_IsNilThread(tid)) {
		printf( "Error: out of thread IDs\n");
		return false;
	    }
	    // create thread
	    L4_Word_t utcb = get_l4thread_manager()->utcb_allocate();
	    if (!utcb) {
		printf( "Error: out of UTCB space\n");
		get_l4thread_manager()->thread_id_release(tid);
		return false;
	    }

	    // XXX check if space exists
	    // XXX space creation and add to task_info_t
	    L4_ThreadId_t space_tid = L4_nilthread;
	    threads[i].ti->set_space_tid(space_tid);

	    L4_Word_t errcode;
	    L4_Word_t prio = ~0UL;
	    errcode = ThreadControl(tid,
				    //L4_Myself(),
				    space_tid,
				    get_vcpu().monitor_gtid,
				    get_vcpu().monitor_gtid, //pager_tid,
				    utcb,
				    prio);
	    if (errcode != L4_ErrOk) {
		printf( "Error: unable to create a thread, L4 error: %s", L4_ErrString(errcode));
		get_l4thread_manager()->thread_id_release(tid);
		return false;
	    }

	    
	    if (!L4_Set_ProcessorNo(tid, get_vcpu().get_pcpu_id()))
	    {
		printf( "Error: unable to set a thread's processor to %d, L4 error: %s", 
			get_vcpu().get_pcpu_id(), L4_ErrString(errcode));
		get_l4thread_manager()->thread_id_release( tid );
		return false;
	    }

	    // Set the thread's starting SP and starting IP.
	    L4_Word_t resume_sp = threads[i].mrs.get(OFS_MR_SAVE_ESP);
	    L4_Word_t resume_ip = threads[i].mrs.get(OFS_MR_SAVE_EIP);
	    L4_Word_t result;
	    L4_ThreadId_t dummy_tid;
	    L4_ThreadId_t local_tid = L4_ExchangeRegisters( tid, (3 << 3) | (1 << 6),
							    resume_sp, resume_ip, 0,
							    //L4_Word_t(l4thread),
							    0, // XXX
							    L4_nilthread,
							    &result, &result,
							    &result, &result,
							    &result,
							    &dummy_tid );
	    if( L4_IsNilThread(local_tid) )
	    {
		printf( "Error: unable to setup a thread, L4 error: %s", L4_ErrString(errcode));
		get_l4thread_manager()->thread_id_release( tid );
		return false;
	    }
	    bool mbt = get_vcpu().add_vcpu_thread(tid, local_tid);
	    ASSERT(mbt);
	    
	    threads[i].tid = local_tid;
	    threads[i].ti->set_space_tid(space_tid);

	} // not nil thread
    } // foreach thread
    return true;
}


static word_t l4_threadcount;

thread_info_t *task_info_t::allocate_vcpu_thread()
{

    L4_Error_t errcode;
    vcpu_t &vcpu = get_vcpu();
    L4_ThreadId_t controller_tid = vcpu.main_gtid;
   

    // Allocate a thread ID.
    L4_ThreadId_t tid = get_l4thread_manager()->thread_id_allocate();
    ASSERT(!L4_IsNilThread(tid));
    
    L4_Word_t utcb = utcb_area_base + (vcpu.cpu_id * task_info_t::utcb_size);
    
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
	utcb_fp = L4_Fpage( utcb_area_base, utcb_area_size);
	kip_fp = L4_Fpage( L4_Address(utcb_fp) + L4_Size(utcb_fp), L4_KipAreaSize(kip));
	
	errcode = SpaceControl( tid, 0, kip_fp, utcb_fp, L4_nilthread );
	if ( errcode != L4_ErrOk )
	    PANIC( "Failed to create address space, TID %t, L4 error %s", 
		   tid,  L4_ErrString(errcode) );
	
	space_tid = tid;	
    }
    
    ASSERT(space_tid != L4_nilthread);

    dprintf(debug_task, "alloc task %x TID %t space TID %t kip %x utcb %x\n",
	    vcpu_thread[vcpu.cpu_id], tid, space_tid, utcb, L4_Address(kip_fp));

    // Create the L4 thread.
    errcode = ThreadControl( tid, space_tid, controller_tid, controller_tid, utcb );
    ++l4_threadcount;
    
    if( errcode != L4_ErrOk )
	    PANIC( "Failed to create user thread, TID %t, utcb %x, L4 error %s", 
		   tid, utcb, L4_ErrString(errcode) );
    
    // Set the thread's exception handler and configure cxfer messages via exregs
    L4_Word_t dummy;
    L4_ThreadId_t local_tid, dummy_tid;
    L4_Msg_t ctrlxfer_msg;
    L4_Word64_t fault_id_mask = (1<<2) | (1<<3) | (1<<5);
    L4_Word_t fault_mask = THREAD_FAULT_MASK;
    
    L4_Clear(&ctrlxfer_msg);
    L4_MsgAppendWord (&ctrlxfer_msg, controller_tid.raw);
    L4_AppendFaultConfCtrlXferItems(&ctrlxfer_msg, fault_id_mask, fault_mask, false);
    L4_Load(&ctrlxfer_msg);
    
    local_tid = L4_ExchangeRegisters( tid, L4_EXREGS_EXCHANDLER_FLAG | L4_EXREGS_CTRLXFER_CONF_FLAG, 
				      0, 0, 0, 0, L4_nilthread, 
				      &dummy, &dummy, &dummy, &dummy, &dummy,
				      &dummy_tid );
    
    if( L4_IsNilThread(local_tid) ) {
	PANIC("Failed to configure user thread's cxfer messages exception handler\n");
    }
    
    if (!L4_Set_ProcessorNo(tid, get_vcpu().get_pcpu_id()))
	PANIC( "Failed to set user thread %t processor number to %d ", tid, vcpu.get_pcpu_id());
    

    dprintf(debug_task, "allocate user thread %t\n", tid);
    vcpu_thread[vcpu.cpu_id]->state = thread_state_startup;
   
    return vcpu_thread[vcpu.cpu_id];
}

void task_info_t::free( )
{

    L4_Error_t errcode;
 
    ASSERT(vcpu_ref_count == 0);
    
    dprintf(debug_task, "delete task %x space TID %t count %d\n",
		this, space_tid, vcpu_thread_count);
    
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
	    get_l4thread_manager()->thread_id_release( tid );
	    get_thread_manager().deallocate( vcpu_thread[id] );
	    vcpu_thread[id] = NULL;

	}
    }
    
    //printf( l4_threadcount << "-\n");
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

    //printf( "*");

    vcpu_t vcpu = get_vcpu();
    thread_info_t *vcpu_info = vcpu_thread[vcpu.cpu_id]; 

    if (vcpu_info == NULL)
    {
	vcpu_info = allocate_vcpu_thread();
	ASSERT(vcpu_info);
	vcpu_info->state = thread_state_preemption;
	vcpu_info->mrs.set_msg_tag( (L4_MsgTag_t) { raw : 0xffd00000 } );
    }
 
    L4_Word_t utcb = utcb_area_base + (vcpu.cpu_id * task_info_t::utcb_size) + 0x100;
   
    dprintf(debug_unmap, "commit helper %x vci %x tid %t ct %d\n",
	    this, vcpu_info, vcpu_info->get_tid(), unmap_count);

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
    L4_GPRegsCtrlXferItem_t ctrlxfer;
    ctrlxfer.regs.eip = (word_t) afterburner_helper;
    ctrlxfer.regs.eflags = 0x3202;
    ctrlxfer.regs.edi = utcb;
    ctrlxfer.regs.esi = unmap_pages[0].raw;
    ctrlxfer.regs.edx = unmap_pages[1].raw;
    ctrlxfer.regs.esp = unmap_pages[2].raw;
    ctrlxfer.regs.ecx = unmap_pages[3].raw;
    ctrlxfer.regs.eax = 0x8000003f + unmap_count;
    ctrlxfer.regs.ebx = L4_Address(kip_fp)+ kip->ThreadSwitch;	
    ctrlxfer.regs.ebp = L4_Address(kip_fp)+ kip->Unmap;
    L4_CtrlXferItemInit(&ctrlxfer.item, L4_CTRLXFER_GPREGS_ID); 
    ctrlxfer.item.num_regs = L4_CTRLXFER_GPREGS_SIZE;
    ctrlxfer.item.mask = 0x3ff;
    L4_LoadMRs( 1 + untyped, L4_CTRLXFER_GPREGS_SIZE+1, ctrlxfer.raw);
    tag.X.u = untyped;
    tag.X.t = L4_CTRLXFER_GPREGS_SIZE+1;
    
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
		
		printf( "VMEXT Bug invalid helper thread state\n");
		DEBUGGER_ENTER("VMEXT BUG");
		
	    }
	    break;
	}
    } 
}

#endif /* defined(CONFIG_VSMP) */

