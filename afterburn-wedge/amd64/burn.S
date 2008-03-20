/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe and
 *                      National ICT Australia (NICTA)
 *
 * File path:     afterburn-wedge/ia32/burn.S
 * Description:   Trampolines for entering C code from the afterburned
 *                guest OS.
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

#include INC_ARCH(frontend.S)

burn_func burn_unimplemented
	burn_call_c afterburn_cpu_unimplemented_ext

burn_func burn_out
  burn_call_c afterburn_cpu_write_port_ext
  //burn_call_c burn_out_impl

burn_func burn_in
  burn_call_c afterburn_cpu_read_port_ext

burn_func burn_rdmsr
	burn_call_c afterburn_cpu_read_msr_ext

burn_func burn_lgdt
  burn_call_c afterburn_cpu_write_gdt32_ext
  //burn_call_c burn_lgdt_impl



/* dummies */

burn_func burn_wrmsr
  burn_call_c burn_wrmsr_impl 

burn_func burn_interruptible_hlt
  burn_call_c burn_interruptible_hlt_impl 

burn_func burn_cpuid
  burn_call_c burn_cpuid_impl 

burn_func burn_int
  burn_call_c burn_int_impl 

burn_func burn_iret
  burn_call_c burn_iret_impl 

burn_func burn_lret
  burn_call_c burn_lret_impl 

burn_func burn_lidt
  burn_call_c burn_lidt_impl 

burn_func burn_invlpg
  burn_call_c burn_invlpg_impl 

burn_func burn_lldt
  burn_call_c burn_lldt_impl 

burn_func burn_ltr
  burn_call_c burn_ltr_impl 

burn_func burn_str
  burn_call_c burn_str_impl 

burn_func burn_clts
  burn_call_c burn_clts_impl 

burn_func burn_cli
  burn_call_c burn_cli_impl 

burn_func burn_sti
  burn_call_c burn_sti_impl 

burn_func burn_deliver_interrupt
  burn_call_c burn_deliver_interrupt_impl 

burn_func burn_popf
  burn_call_c burn_popf_impl 

burn_func burn_pushf
  burn_call_c burn_pushf_impl 

burn_func burn_write_cr0
  burn_call_c burn_write_cr0_impl 

burn_func burn_write_cr2
  burn_call_c burn_write_cr2_impl 

burn_func burn_write_cr3
  burn_call_c burn_write_cr3_impl 

burn_func burn_write_cr4
  burn_call_c burn_write_cr4_impl 

burn_func burn_read_cr
  burn_call_c burn_read_cr_impl 

burn_func burn_read_cr0
  burn_call_c burn_read_cr0_impl 

burn_func burn_read_cr2
  burn_call_c burn_read_cr2_impl 

burn_func burn_read_cr3
  burn_call_c burn_read_cr3_impl 

burn_func burn_read_cr4
  burn_call_c burn_read_cr4_impl 

burn_func burn_write_dr
  burn_call_c burn_write_dr_impl 

burn_func burn_read_dr
  burn_call_c burn_read_dr_impl 

burn_func burn_ud2
  burn_call_c burn_ud2_impl 

burn_func burn_write_cs
  burn_call_c burn_write_cs_impl 

burn_func burn_write_ds
  burn_call_c burn_write_ds_impl 

burn_func burn_write_es
  burn_call_c burn_write_es_impl 

burn_func burn_write_fs
  burn_call_c burn_write_fs_impl 

burn_func burn_write_gs
  burn_call_c burn_write_gs_impl 

burn_func burn_write_ss
  burn_call_c burn_write_ss_impl 

burn_func burn_read_cs
  burn_call_c burn_read_cs_impl 

burn_func burn_read_ds
  burn_call_c burn_read_ds_impl 

burn_func burn_read_es
  burn_call_c burn_read_es_impl 

burn_func burn_read_fs
  burn_call_c burn_read_fs_impl 

burn_func burn_read_gs
  burn_call_c burn_read_gs_impl 

burn_func burn_read_ss
  burn_call_c burn_read_ss_impl 

burn_func burn_lss
  burn_call_c burn_lss_impl 

burn_func burn_invd
  burn_call_c burn_invd_impl 

burn_func burn_wbinvd
  burn_call_c burn_wbinvd_impl 

burn_func burn_mov_tofsofs
  burn_call_c burn_mov_tofsofs_impl 

burn_func burn_mov_fromfsofs
  burn_call_c burn_mov_fromfsofs_impl 

burn_func burn_mov_tofsofs_eax
  burn_call_c burn_mov_tofsofs_eax_impl 

burn_func burn_mov_togsofs
  burn_call_c burn_mov_togsofs_impl 

burn_func burn_mov_fromgsofs
  burn_call_c burn_mov_fromgsofs_impl 

burn_func burn_interrupt_redirect
  burn_call_c burn_interrupt_redirect_impl 
