/*********************************************************************
 *
 * Copyright (C) 2005, University of Karlsruhe
 *
 * File path:     afterburn-wedge/common/crtbegin.cc
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

unsigned char *afterburn_prezero_start SECTION(".bss");
void (*afterburn_ctors_start)(void) SECTION(".ctors");
void (*afterburn_dtors_start)(void) SECTION(".dtors");

unsigned long _burn_symbols_start[0] SECTION(".burn_symbols");

#if defined(CONFIG_BURN_COUNTERS)
unsigned long __burn_counters_start[0] SECTION(".burn_counters");
unsigned long __burn_counter_regions_start[0] SECTION(".burn_counter_regions");
unsigned long _perf_counters_start[0] SECTION(".perf_counters");
#endif
#if defined(CONFIG_VMI_SUPPORT)
unsigned long _vmi_patchups_start[0] SECTION(".vmi_patchups");
#endif
#if defined(CONFIG_BURN_PROFILE)
unsigned long _burn_profile_start[0] SECTION(".burn_prof_counters");
#endif
