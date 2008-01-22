/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/include/string.h
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

#ifndef __STRING_H__
#define __STRING_H__
#include INC_ARCH(types.h)

#ifdef __cplusplus
extern "C" {
#endif
    extern unsigned strlen( const char *str );
    extern int strcmp( const char *s1, const char *s2 );
    extern int strncmp(const char *s1, const char *s2, int n);
    extern char * strstr(const char *s, const char *find);;
    extern char *strncpy( char *dest, const char *src, word_t n );
    extern unsigned long strtoul(const char* nptr, char** endptr, int base);
    
    extern void *memmove( void *dest, const void *src, word_t n );
    extern void memcpy( void *dest, const void *src, unsigned long n );
    extern void memzero( void *start, unsigned long size );
    extern void *memset( void *mem, int c, unsigned long size );
    extern void zero_mem( void *dest, word_t size );
    extern void page_zero( void *start, unsigned long size );

#ifdef __cplusplus
}
#endif

/* Some ctype like stuff to support strtoul */
#define isspace(c)      ((c == ' ') || (c == '\t'))
#define ULONG_MAX       (~0UL)
#define isdigit(c)      ((c) >= '0' && (c) <= '9')
#define islower(c)      (((c) >= 'a') && ((c) <= 'z'))
#define isupper(c)      (((c) >= 'A') && ((c) <= 'Z'))
#define isalpha(c)      (islower(c) || isupper(c))


#endif /* !__STRING_H__ */
