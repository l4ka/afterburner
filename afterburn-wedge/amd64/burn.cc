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

extern "C" void burn_wrmsr(void){UNIMPLEMENTED();}
extern "C" void burn_rdmsr(void){UNIMPLEMENTED();}
extern "C" void burn_interruptible_hlt(void){UNIMPLEMENTED();}
extern "C" void burn_out(void){UNIMPLEMENTED();}
extern "C" void burn_cpuid(void){UNIMPLEMENTED();}
extern "C" void burn_in(void){UNIMPLEMENTED();}
extern "C" void burn_int(void){UNIMPLEMENTED();}
extern "C" void burn_iret(void){UNIMPLEMENTED();}
extern "C" void burn_lret(void){UNIMPLEMENTED();}
extern "C" void burn_lidt(void){UNIMPLEMENTED();}
extern "C" void burn_lgdt(void){UNIMPLEMENTED();}
extern "C" void burn_invlpg(void){UNIMPLEMENTED();}
extern "C" void burn_lldt(void){UNIMPLEMENTED();}
extern "C" void burn_ltr(void){UNIMPLEMENTED();}
extern "C" void burn_str(void){UNIMPLEMENTED();}
extern "C" void burn_clts(void){UNIMPLEMENTED();}
extern "C" void burn_cli(void){UNIMPLEMENTED();}
extern "C" void burn_sti(void){UNIMPLEMENTED();}
extern "C" void burn_deliver_interrupt(void){UNIMPLEMENTED();}
extern "C" void burn_popf(void){UNIMPLEMENTED();}
extern "C" void burn_pushf(void){UNIMPLEMENTED();}
extern "C" void burn_write_cr0(void){UNIMPLEMENTED();}
extern "C" void burn_write_cr2(void){UNIMPLEMENTED();}
extern "C" void burn_write_cr3(void){UNIMPLEMENTED();}
extern "C" void burn_write_cr4(void){UNIMPLEMENTED();}
extern "C" void burn_read_cr(void){UNIMPLEMENTED();}
extern "C" void burn_read_cr0(void){UNIMPLEMENTED();}
extern "C" void burn_read_cr2(void){UNIMPLEMENTED();}
extern "C" void burn_read_cr3(void){UNIMPLEMENTED();}
extern "C" void burn_read_cr4(void){UNIMPLEMENTED();}
extern "C" void burn_write_dr(void){UNIMPLEMENTED();}
extern "C" void burn_read_dr(void){UNIMPLEMENTED();}
extern "C" void burn_ud2(void){UNIMPLEMENTED();}
extern "C" void burn_unimplemented(void){UNIMPLEMENTED();}

extern "C" void burn_write_cs(void){UNIMPLEMENTED();}
extern "C" void burn_write_ds(void){UNIMPLEMENTED();}
extern "C" void burn_write_es(void){UNIMPLEMENTED();}
extern "C" void burn_write_fs(void){UNIMPLEMENTED();}
extern "C" void burn_write_gs(void){UNIMPLEMENTED();}
extern "C" void burn_write_ss(void){UNIMPLEMENTED();}
extern "C" void burn_read_cs(void){UNIMPLEMENTED();}
extern "C" void burn_read_ds(void){UNIMPLEMENTED();}
extern "C" void burn_read_es(void){UNIMPLEMENTED();}
extern "C" void burn_read_fs(void){UNIMPLEMENTED();}
extern "C" void burn_read_gs(void){UNIMPLEMENTED();}
extern "C" void burn_read_ss(void){UNIMPLEMENTED();}
extern "C" void burn_lss(void){UNIMPLEMENTED();}
extern "C" void burn_invd(void){UNIMPLEMENTED();}
extern "C" void burn_wbinvd(void){UNIMPLEMENTED();}

extern "C" void burn_mov_tofsofs(void){UNIMPLEMENTED();}
extern "C" void burn_mov_fromfsofs(void){UNIMPLEMENTED();}
extern "C" void burn_mov_tofsofs_eax(void){UNIMPLEMENTED();}
extern "C" void burn_mov_togsofs(void){UNIMPLEMENTED();}
extern "C" void burn_mov_fromgsofs(void){UNIMPLEMENTED();}

extern "C" void device_8259_0x20_in(void){UNIMPLEMENTED();}
extern "C" void device_8259_0x21_in(void){UNIMPLEMENTED();}
extern "C" void device_8259_0xa0_in(void){UNIMPLEMENTED();}
extern "C" void device_8259_0xa1_in(void){UNIMPLEMENTED();}
extern "C" void device_8259_0x20_out(void){UNIMPLEMENTED();}
extern "C" void device_8259_0x21_out(void){UNIMPLEMENTED();}
extern "C" void device_8259_0xa0_out(void){UNIMPLEMENTED();}
extern "C" void device_8259_0xa1_out(void){UNIMPLEMENTED();}
