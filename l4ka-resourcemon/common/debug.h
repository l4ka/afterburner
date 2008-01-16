/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     l4ka-resourcemon/common/debug.h
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
#ifndef __L4KA_RESOURCEMON__COMMON__DEBUG_H__
#define __L4KA_RESOURCEMON__COMMON__DEBUG_H__

#define L4_TRACEBUFFER
#define L4_PERFMON
#define L4_CONFIG_CPU_IA32_P4
#include <l4/kdebug.h>
#include <l4/tracebuffer.h>

#define DBG_LEVEL	2
#define TRACE_LEVEL	5
#define PREPAD		"             "
#define PREFIX		"\e[1m\e[33mresourcemon:\e[0m "


extern "C" int trace_printf(const char* format, ...);		
extern "C" int l4kdb_printf(const char* format, ...);
extern bool l4_tracebuffer_enabled;

void set_console_prefix(char* prefix);		

#define dprintf(n,a...)						\
    do								\
    {								\
	if(DBG_LEVEL>n)						\
	    l4kdb_printf(a);					\
	if (TRACE_LEVEL>n && l4_tracebuffer_enabled)		\
	    trace_printf(a, L4_TRACEBUFFER_MAGIC);		\
    } while(0)

#define  printf(x...)	dprintf(0, x)

extern "C" int dbg_printf(const char* format, ...);		

extern void assert_hex_to_str( unsigned long val, char *s);
extern void assert_dec_to_str(unsigned long val, char *s);

#define ASSERT(x)					\
    do {						\
	if(EXPECT_FALSE(!(x))) {			\
	    extern char assert_string[512];		\
	    char *_d = &assert_string[11];		\
	    char *_s = NULL;				\
	    char _l[10];				\
	    assert_dec_to_str(__LINE__, _l);		\
	    _s = MKSTR(x);				\
	    while (*_s)	*_d++ = *_s++;			\
	    *_d++ = ' ';				\
	    _s = __FILE__;				\
	    while (*_s)	*_d++ = *_s++;			\
	    *_d++ = ' ';				\
	    _s = _l;					\
	    while (*_s)	*_d++ = *_s++;			\
	    *_d++ = '\n';				\
	    *_d++ = 0;					\
	    if (l4_tracebuffer_enabled)			\
		L4_Tbuf_RecordEvent (0, assert_string);	\
	    L4_KDB_PrintString(assert_string);		\
	    L4_KDB_Enter("panic");			\
	}						\
    } while(0)


#define UNIMPLEMENTED() PANIC("UNIMPLEMENTED");


#endif	/* __L4KA_RESOURCEMON__COMMON__DEBUG_H__ */
