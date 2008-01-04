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

#define L4_PERFMON
#define L4_TRACEBUFFER

#include <l4/kdebug.h>
#include <l4/tracebuffer.h>

#define DBG_LEVEL	2
#define TRACE_LEVEL	5
#define PREFIX		"resourcemon: "
#define PREPAD		"             "



extern "C" void putc(const char c);

extern "C" int trace_printf(const char* format, ...);		
extern "C" int dbg_printf(const char* format, ...);		

#define dprintf(n,a...)						\
    do								\
    {								\
	if(DBG_LEVEL>n)						\
	    dbg_printf(a);					\
	if (TRACE_LEVEL>n)					\
	    trace_printf(a, L4_TRACEBUFFER_MAGIC);		\
    } while(0)

#define  printf(x...)	dprintf(0, x)

# define ASSERT(x)							\
do {									\
    if (EXPECT_FALSE(! (x))) {						\
	dprintf(0,  "Assertion " #x " failed in file %s line %d fn %x",	\
		__FILE__, __LINE__,  __builtin_return_address((0)));	\
	L4_KDB_Enter ("assert");					\
    }									\
} while(false)
#define PANIC(a) L4_KDB_Enter("#a");

#endif	/* __L4KA_RESOURCEMON__COMMON__DEBUG_H__ */
