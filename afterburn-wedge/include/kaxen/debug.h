/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/kaxen/debug.h
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
 * $Id: debug.h,v 1.8 2006/01/11 17:59:37 store_mrs Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__KAXEN__DEBUG_H__
#define __AFTERBURN_WEDGE__INCLUDE__KAXEN__DEBUG_H__

#include INC_WEDGE(console.h)

struct xen_frame_t;

#define DEBUG_STREAM hiostream_kaxen_t

extern NORETURN void panic( xen_frame_t *frame=0 );

#if defined(CONFIG_DEBUGGER)
extern "C" void __attribute__(( regparm(1) ))
debugger_enter( xen_frame_t *frame=0 );
#define DEBUGGER_ENTER(frame) debugger_enter(frame)
#else
#define DEBUGGER_ENTER(frame) do {} while(0)
#endif

#define PANIC(sequence, frame...)					\
    do {								\
	con << sequence << "\nFile: " __FILE__				\
	    << ':' << __LINE__ << "\nFunc: " << __func__  << '\n';	\
	panic(frame);							\
    } while(0)

#if defined(CONFIG_OPTIMIZE)
#define ASSERT(x, frame...)
#else
#define ASSERT(x, frame...)		\
    do { 				\
	if(EXPECT_FALSE(!(x))) { 	\
    	    con << "Assertion: " MKSTR(x) ",\nfile " __FILE__ \
	        << ':' << __LINE__ << ",\nfunc " \
	        << __func__ << '\n';	\
	    panic(frame);		\
	}				\
    } while(0)
#endif

#define UNIMPLEMENTED() PANIC("UNIMPLEMENTED");


#if defined(CONFIG_DEBUGGER)
extern bool dbg_pgfault_perf_resolve( xen_frame_t *frame );
#endif

#endif	/* __AFTERBURN_WEDGE__INCLUDE__KAXEN__DEBUG_H__ */
