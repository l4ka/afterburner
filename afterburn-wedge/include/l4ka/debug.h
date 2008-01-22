/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/debug.h
 * Description:   Debug support.
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
 * $Id: debug.h,v 1.7 2006/03/29 14:14:38 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4KA__DEBUG_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4KA__DEBUG_H__

#include INC_WEDGE(console.h)

#include <l4/tracebuffer.h>
#include <l4/thread.h>
#include <l4/kdebug.h>
#include <l4/types.h>

extern bool l4_tracebuffer_enabled;

#define DEBUG_STREAM hiostream_kdebug_t

#define DEBUGGER_ENTER(a)				\
    do {						\
	volatile char *__c = (volatile char *) &a;	\
	while (*__c++) ;				\
	L4_KDB_Enter(a);				\
    } while (0);

extern NORETURN void panic( void );

extern void debug_hex_to_str( unsigned long val, char *s );
extern void debug_dec_to_str(unsigned long val, char *s);

#define PANIC(seq...)				\
    do {					\
	dbg_printf(0, seq);			\
	DEBUGGER_ENTER("panic");		\
	panic();				\
    } while(0)

#if defined(CONFIG_OPTIMIZE)
#define ASSERT(x)
#else
#define ASSERT(x)					\
    do {						\
	if(EXPECT_FALSE(!(x))) {			\
	    extern char assert_string[512];		\
	    char *_d = &assert_string[11];		\
	    char *_s = NULL;				\
	    char _l[10];				\
	    debug_dec_to_str(__LINE__, _l);		\
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
	    DEBUGGER_ENTER("panic");			\
	    panic();					\
	}						\
    } while(0)
#endif

#define UNIMPLEMENTED() PANIC("UNIMPLEMENTED");

class thread_info_t;
extern void dump_syscall(thread_info_t *thread_info, bool dir);

#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__DEBUG_H__ */

