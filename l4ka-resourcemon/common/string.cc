/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:	resourcemon/string.cc
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

#include <l4/types.h>
#include <common/string.h>

void zero_mem( void *dest, L4_Word_t size )
{
    L4_Word_t *data = (L4_Word_t *)dest;

    while( size >= sizeof(L4_Word_t) )
    {
	*data = 0;
	size -= sizeof(L4_Word_t);
	data++;
    }
}

void *memcpy( void *dest, const void *src, L4_Word_t n )
{
    for( L4_Word_t i = 0; i < n; i++ )
	((L4_Word8_t *)dest)[i] = ((L4_Word8_t *)src)[i];

    return dest;
}

void *memset( void *s, L4_Word8_t c, L4_Word_t n )
{
    for( L4_Word_t i = 0; i < n; i++ )
	((L4_Word8_t *)s)[i] = c;
    return s;
}

int strlen(const char *str)
{
    int len = 0;
    while (0 != str[len]) {
	len++;
    }
    return len;
}

int strcmp( const char *str1, const char *str2 )
{
    while( *str1 && *str2 ) 
    {
	if( *str1 < *str2 )
	    return -1;
	if( *str1 > *str2 )
	    return 1;
	str1++;
	str2++;
    }
    if( *str2 )
	return -1;
    if( *str1 )
	return 1;
    return 0;
}

char *strncpy( char *dest, const char *src, L4_Word_t n )
{
    L4_Word_t i;
    for( i = 0; (i < n) && src[i]; i++ )
	dest[i] = src[i];
    dest[i] = '\0';
    return dest;
}


/*
 * For the remainder of this file:
 *      
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 */       

/*
 * Find the first occurrence of find in s.
 */
char *
strstr(const char *s, const char *find)
{
	char c, sc;
	int len;

	if ((c = *find++) != 0) {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (0);
			} while (sc != c);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}


/*
 * Convert a string to an unsigned long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
unsigned long
strtoul(const char* nptr, char** endptr, int base)
{
	const char *s;
	unsigned long acc, cutoff;
	int c;
	int neg, any, cutlim;

	/*
	 * See strtol for comments as to the logic used.
	 */
	s = nptr;
	do {
		c = (unsigned char) *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else {
		neg = 0;
		if (c == '+')
			c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	cutoff = ULONG_MAX / (unsigned long)base;
	cutlim = ULONG_MAX % (unsigned long)base;
	for (acc = 0, any = 0;; c = (unsigned char) *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0)
			continue;
		if (acc > cutoff || acc == cutoff && c > cutlim) {
			any = -1;
			acc = ULONG_MAX;
		} else {
			any = 1;
			acc *= (unsigned long)base;
			acc += c;
		}
	}
	if (neg && any > 0)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *) (any ? s - 1 : nptr);
	return (acc);
}
    
int
strncmp(const char *s1, const char *s2, int n)
{

	if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(unsigned char *)s1 - *(unsigned char *)--s2);
		if (*s1++ == 0)
			break;
	} while (--n != 0);
	return (0);
}

