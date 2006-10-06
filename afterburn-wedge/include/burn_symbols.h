/*********************************************************************
 *                
 * Copyright (C) 2005,  University of Karlsruhe
 *                
 * File path:     afterburn-wedge/include/burn_symbols.h
 * Description:   Support for dynamic linking against the wedge.
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
 * $Id: burn_symbols.h,v 1.3 2005/07/14 20:29:09 joshua Exp $
 *                
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__BURN_SYMBOLS_H__
#define __AFTERBURN_WEDGE__INCLUDE__BURN_SYMBOLS_H__

#include INC_ARCH(types.h)

struct burn_wedge_header_t
{
    static const word_t current_version = 1;
    word_t version;
};

struct burn_symbol_t
{
    const void *ptr;
    const char *name;
    word_t usage_count;
    burn_symbol_t *next; // For use by the hash table, to avoid dynamic memory.
};

#define DECLARE_BURN_SYMBOL(c)						\
    char * _burn_symbol_string__##c SECTION(".burn_symbol_strings") =	\
	MKSTR(c);							\
    burn_symbol_t _burn_symbol__##c SECTION(".burn_symbols") =		\
	{ (const void *)&c, _burn_symbol_string__##c, 0, 0 };


class burn_symbols_hash_t
{
public:
    void init();
    burn_symbol_t *lookup_symbol( const char *symbol );

private:
    static const word_t size = 8;
    burn_symbol_t * table[size];

    word_t compute_symbol_hash( const char *symbol )
    {
	word_t hash = 0;
	while( *symbol ) {
	    hash += *symbol;
	    symbol++;
	}
	return hash % size;
    }

    static inline burn_symbol_t *get_list_start()
    {
	extern word_t _burn_symbols_start[];
	return (burn_symbol_t *)_burn_symbols_start;
    }

    static inline burn_symbol_t *get_list_end()
    {
	extern word_t _burn_symbols_end[];
	return (burn_symbol_t *)_burn_symbols_end;
    }
};

INLINE burn_symbols_hash_t & get_burn_symbols()
{
    extern burn_symbols_hash_t burn_symbols_hash;
    return burn_symbols_hash;
}

#endif	/* __AFTERBURN_WEDGE__INCLUDE__BURN_SYMBOLS_H__ */
