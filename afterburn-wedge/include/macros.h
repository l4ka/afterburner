/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/macros.h
 * Description:   Generic macros.  Should be safe to include in any 
 *                C and asm file.
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
 * $Id: macros.h,v 1.9 2005/12/16 12:06:20 joshua Exp $
 *
 ********************************************************************/

#ifndef __AFTERBURN_WEDGE__INCLUDE__MACROS_H__
#define __AFTERBURN_WEDGE__INCLUDE__MACROS_H__

/*
 * Computed include paths for header files.
 */

#define INC_ARCH(x)		<CONFIG_ARCH/x>
#define INC_CPU(x)		<CONFIG_ARCH/CONFIG_CPU/x>
#define INC_WEDGE(x)		<CONFIG_WEDGE/x>

#if !defined(ASSEMBLY)

/* gcc attributes. */
#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 3)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#define INLINE extern inline

#if (__GNUC__ >= 3)
#define EXPECT_FALSE(x)		__builtin_expect((x), false)
#define EXPECT_TRUE(x)		__builtin_expect((x), true)
#define EXPECT_VALUE(x,val)	__builtin_expect((x), (val))
#else
#define EXPECT_FALSE(x)		(x)
#define EXPECT_TRUE(x)		(x)
#define EXPECT_VALUE(x,val)	(x)
#endif

#define SECTION(x) __attribute__((section(x)))
#define NORETURN __attribute__((noreturn))
#define PAGEALIGNED __attribute__((aligned=(1 << CONFIG_PAGEBITS)))
#define ALIGNED(x) __attribute__((aligned(x)))
#define WEAK __attribute__(( weak ))
#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1)
#define NOINLINE __attribute__ ((noinline))
#else
#define NOINLINE
#endif

/* Convenience functions for memory sizes. */
#define KB(x)   ((typeof(x)) (word_t(x) * 1024))
#define MB(x)   ((typeof(x)) (word_t(x) * 1024*1024))
#define GB(x)   ((typeof(x)) (word_t(x) * 1024*1024*1024))

#define offsetof(type, field) (word_t(&((type *)0x1000)->field) - 0x1000)

#endif	/* ASSEMBLY */


/* Turn preprocessor symbol definition into string */
#define MKSTR(sym)      MKSTR2(sym)
#define MKSTR2(sym)     #sym

/* Safely "append" an UL suffix for also asm values */
#if defined(ASSEMBLY)
#define __UL(x)         x
#else
#define __UL(x)         x##UL
#endif

#ifndef NULL
#define NULL	0
#endif

#define MASK_BITS(w, b)  (w & ~(1UL << b)-1)



#endif	/* __AFTERBURN_WEDGE__INCLUDE__MACROS_H__ */
