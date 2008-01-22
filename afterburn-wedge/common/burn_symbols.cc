/*********************************************************************
 *                
 * Copyright (C) 2005,  University of Karlsruhe
 *                
 * File path:     afterburn-wedge/common/burn_symbols.cc
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
 * $Id: burn_symbols.cc,v 1.1 2005/07/14 17:21:21 joshua Exp $
 *                
 ********************************************************************/

#include <burn_symbols.h>
#include <string.h>

burn_symbols_hash_t burn_symbols_hash;

void burn_symbols_hash_t::init()
{
    // Zero the hash table.
    for( word_t i = 0; i < size; i++ )
	table[i] = 0;

    // Walk the global array of symbol entries and insert into the hash table.
    burn_symbol_t *entry = get_list_start();
    while( entry != get_list_end() ) {
	word_t hash = compute_symbol_hash( entry->name );
	entry->next = table[hash];
	table[hash] = entry;
	entry++;
    }
}

burn_symbol_t * burn_symbols_hash_t::lookup_symbol( const char *symbol )
{
    burn_symbol_t *entry = table[ compute_symbol_hash(symbol) ];
    while( entry ) {
	if( !strcmp(entry->name, symbol) )
	    return entry;
	entry = entry->next;
    }

    return 0;
}

