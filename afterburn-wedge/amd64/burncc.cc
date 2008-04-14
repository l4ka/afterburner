/*********************************************************************
 *
 * Copyright (C) 2008,  University of Karlsruhe
 * Copyright (C) 2008,  Tom Bachmann
 *
 * File path:     afterburn-wedge/amd64/burn.cc
 * Description:   Temporary stub code file.
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
#include INC_WEDGE(debug.h)

#define INFO \
    printf("BURN %s: %p\n", __func__, f); \
    printf("burn ret address: %p\n", f->burn_ret_address); \
    printf("frame pointer: %p\n", f->frame_pointer); \
    printf("rax: %p\n", f->rax); \
    printf("rcx: %p\n", f->rcx); \
    printf("rdx: %p\n", f->rdx); \
    printf("rsi: %p\n", f->rsi); \
    printf("rdi: %p\n", f->rdi); \
    printf(" r8: %p\n", f->r8); \
    printf(" r9: %p\n", f->r9); \
    printf("r10: %p\n", f->r10); \
    printf("r11: %p\n", f->r11); \
    printf("guest ret address: %p\n", f->guest_ret_address); \
    printf("params[0]: %p\n", f->params[0]); \
    printf("params[1]: %p\n", f->params[1]);

extern "C" void burn_wrmsr_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_interruptible_hlt_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_cpuid_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_in_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_int_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_iret_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_lret_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_lidt_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_lgdt_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_invlpg_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_lldt_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_ltr_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_str_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_clts_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_cli_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_sti_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_deliver_interrupt_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_popf_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_pushf_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_write_cr0_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_write_cr2_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_write_cr3_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_write_cr4_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_cr_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_cr0_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_cr2_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_cr3_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_cr4_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_write_dr_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_dr_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_ud2_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}

extern "C" void burn_write_cs_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_write_ds_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_write_es_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_write_fs_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_write_gs_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_write_ss_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_cs_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_ds_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_es_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_fs_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_gs_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_read_ss_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_lss_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_invd_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_wbinvd_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}

extern "C" void burn_mov_tofsofs_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_mov_fromfsofs_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_mov_tofsofs_eax_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_mov_togsofs_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}
extern "C" void burn_mov_fromgsofs_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}

extern "C" void burn_interrupt_redirect_impl(burn_clobbers_frame_t* f)
  {INFO UNIMPLEMENTED();}


extern "C" void burn_out_impl(burn_clobbers_frame_t* f)
{
    INFO
    if(f->params[0]==0x1f)
	printf("%s", f->rax);
    else
        UNIMPLEMENTED();
}

#if 0
asm(".text\n"
    ".global burn_out\n"
    "burn_out:\n"

    "  pushq %r11\n"
    "  pushq %r10\n"
    "  pushq %r9\n"
    "  pushq %r8\n"
    "  pushq %rdi\n"
    "  pushq %rsi\n"
    "  pushq %rdx\n"
    "  pushq %rcx\n"
    "  pushq %rax\n"

    "  pushq %rsp\n"
    "  subl $16, (%rsp)\n"
    "  movq (%rsp), %rdi\n"
    "  callq burn_out_impl\n"
    "  popq %rax\n" // skip rsp

    "  popq %rax\n"
    "  popq %rcx\n"
    "  popq %rdx\n"
    "  popq %rsi\n"
    "  popq %rdi\n"
    "  popq %r8\n"
    "  popq %r9\n"
    "  popq %r10\n"
    "  popq %r11\n"

    "  ret\n");
#endif



// other temporary stubs
void backend_sync_deliver_exception( 
	word_t vector, bool old_int_state, 
	bool use_error_code, word_t error_code )
{
    UNIMPLEMENTED();
}

// traps.cc
extern "C" void int_wrapper_0x20(){UNIMPLEMENTED();}
extern "C" void int_wrapper_0x21(){UNIMPLEMENTED();}
extern "C" void wedge_syscall_wrapper_0x69(){UNIMPLEMENTED();}
extern "C" void int_wrapper_0x80(){UNIMPLEMENTED();}
extern "C" void trap_wrapper_kdb(){UNIMPLEMENTED();}
