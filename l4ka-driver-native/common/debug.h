/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     common/debug.h
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
 * $Id$
 *                
 ********************************************************************/
#ifndef __COMMON__DEBUG_H__
#define __COMMON__DEBUG_H__

extern "C" NORETURN void panic( void );

#if defined(cfg_optimize)
#define ASSERT(x)
#else
#define ASSERT(x)						\
    do {							\
	if(EXPECT_FALSE(!(x))) {				\
	    printf( "Assertion: %s,\nfile %s : %u,\nfunc %s\n",	\
		    MKSTR(x), __FILE__, __LINE__, __func__ );	\
	    panic();						\
	}							\
    } while(0)
#endif

#define PANIC(str)						\
    do {							\
	printf( "Panic: %s,\nfile %s : %u,\nfunc %s\n",		\
		str, __FILE__, __LINE__, __func__ );		\
	panic();						\
    } while(0);

#define UNIMPLEMENTED() PANIC("UNIMPLEMENTED");

#endif	/* __COMMON__DEBUG_H__ */
