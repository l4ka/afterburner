/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     include/macros.h
 * Description:   Generic macros.
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
#ifndef __MACROS_H__
#define __MACROS_H__

#if !defined (ASSEMBLY)

#define INLINE extern inline

#define KB(x) ((typeof(x)) (word_t(x) * 1024))
#define MB(x) ((typeof(x)) (word_t(x) * 1024*1024))
#define GB(x) ((typeof(x)) (word_t(x) * 1024*1024*1024))

#if (__GNUC__ >= 3)
#define EXPECT_FALSE(x)		__builtin_expect((x), false)
#define EXPECT_TRUE(x)		__builtin_expect((x), true)
#define EXPECT_VALUE(x,val)	__builtin_expect((x), (val))
#else
#define EXPECT_FALSE(x)		(x)
#define EXPECT_TRUE(x)		(x)
#define EXPECT_VALUE(x,val)	(x)
#endif

#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 4)
#define ATTR_UNUSED_PARAM __attribute__((unused))
#else
#define ATTR_UNUSED_PARAM
#endif

/* gcc attributes. */
#define __noreturn __attribute__((noreturn))
#define __weak __attribute__(( weak ))
#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1)
#define __noinline __attribute__ ((noinline))
#else
#define __noinline
#endif

#endif /* ASSEMBLY */


/* Generic compile features and macros. */
#define MKSTR(sym)              MKSTR2(sym)
#define MKSTR2(sym)             #sym

#ifndef NULL
#define NULL 0
#endif

#define min(a, b)		((a) < (b) ? (a) : (b))
#define max(a, b)		((a) > (b) ? (a) : (b))


#define htonl(A)  ((((u32_t)(A) & 0xff000000) >> 24) | \
                   (((u32_t)(A) & 0x00ff0000) >> 8)  | \
                   (((u32_t)(A) & 0x0000ff00) << 8)  | \
                   (((u32_t)(A) & 0x000000ff) << 24))

extern inline L4_Word_t round_up( L4_Word_t addr, L4_Word_t size )
{
    if (size == 0)
	return addr;
    if( addr % size )
	return (addr + size) & ~(size-1);
    return addr;
}

#endif  /* __MACROS_H__ */
