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

#include <l4/thread.h>
#include <l4/kdebug.h>
#include <console.h>

#define DEBUG_STREAM hiostream_kdebug_t
#define DEBUGGER_ENTER(a) L4_KDB_Enter("debug")

extern NORETURN void panic( void );


#define PANIC(seq...)					\
    do {						\
	printf(seq);					\
	printf("\nfile %s:%d\n", __FILE__, __LINE__);	\
	L4_KDB_Enter("panic");				\
	panic();					\
    } while(0)

#if defined(CONFIG_OPTIMIZE)
#define ASSERT(x)
#else
#define ASSERT(x)					\
    do {						\
	if(EXPECT_FALSE(!(x))) {			\
	    printf("Assertion: %s,\nfile %s:%d\n",	\
		    MKSTR(x), __FILE__, __LINE__);	\
	    L4_KDB_Enter("panic");			\
	    panic();					\
	}						\
    } while(0)
#endif

#define UNIMPLEMENTED() PANIC("UNIMPLEMENTED");

#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__DEBUG_H__ */
