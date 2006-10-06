/*********************************************************************
 *                
 * Copyright (C) 2006,  University of Karlsruhe
 *                
 * File path:     wedge-apps/burn-prof.cc
 * Description:   Correlate the runtime profiler output with function names.
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "ia32/types.h"
#include "elfsimple.h"


class sym_tree_t {
    // A node for building a tree of ELF function symbols, which are indexed
    // by their function start addresses.  The data structure is used for 
    // looking up arbitrary addresses within the functions, to determine
    // the function name for the address.
public:
    elf_sym_t *elf_sym;
    sym_tree_t *left, *right;

    word_t get_address() { return elf_sym->value; }
    word_t get_size()    { return elf_sym->size;  }

    word_t is_match( word_t addr )
    {
	word_t end = get_address() + get_size();
	return (addr >= get_address()) && (addr < end);
    }

    sym_tree_t( elf_sym_t *new_sym )
    {
	elf_sym = new_sym;
	left = right = NULL;
    }
};

sym_tree_t *sym_tree = NULL;

void add_sym( elf_sym_t *sym )
    // Construct a tree of the symbols, ordered by their addresses.
    // It is important that we can quickly lookup symbols by addresses.
{
    sym_tree_t *node = new sym_tree_t(sym);

    if( sym_tree == NULL ) {
	sym_tree = node;
	return;
    }

    sym_tree_t *leaf = sym_tree;
    for(;;)
    {
	if( sym->value < leaf->get_address() )
	{
	    if( leaf->left == NULL ) {
		leaf->left = node;
		return;
	    } else
		leaf = leaf->left;
	}
	else
	{
	    if( leaf->right == NULL ) {
		leaf->right = node;
		return;
	    } else
		leaf = leaf->right;
	}
    }
}

elf_sym_t * sym_lookup( word_t addr )
{
    sym_tree_t *node = sym_tree;
    while( node ) {
	if( node->is_match(addr) )
	    return node->elf_sym;
	else if( addr < node->get_address() )
	    node = node->left;
	else
	    node = node->right;
    }

    return NULL;
}

void usage( const char *progname )
{
    fprintf( stderr, "usage: %s <counters filename> <binary filename>\n",
	    progname );
    exit( 1 );
}

void map_file( const char *path, int *fd, void **ptr, word_t *bytes )
{
    *fd = open( path, O_RDONLY );
    if( *fd < 0 ) {
	fprintf( stderr, "Error opening '%s': %s\n", path, strerror(errno) );
	exit( 1 );
    }

    struct stat stat;
    if( fstat(*fd, &stat) < 0 ) {
	fprintf( stderr, "Error mapping '%s': %s\n", path, strerror(errno) );
	exit( 1 );
    }
    *bytes = stat.st_size;

    *ptr = mmap( NULL, stat.st_size, PROT_READ, MAP_PRIVATE, *fd, 0 );
    if( *ptr == MAP_FAILED ) {
	fprintf( stderr, "Error mapping '%s': %s\n", path, strerror(errno) );
	exit( 1 );
    }
}

int main( int argc, const char *argv[] )
{
    const char *path_counters = NULL;
    const char *path_binary = NULL;

    if( argc != 3 )
	usage( argv[0] );
    path_counters = argv[1];
    path_binary = argv[2];

    int fd_counters, fd_binary;
    void *vm_counters, *vm_binary;
    word_t bytes_counters, bytes_binary;

    map_file( path_counters, &fd_counters, &vm_counters, &bytes_counters );
    map_file( path_binary, &fd_binary, &vm_binary, &bytes_binary );

    // The profile counters, extracted from a running system.
    const word_t *counters = (const word_t *)vm_counters;
    // The binary corresponding to the profile counters.
    elf_ehdr_t *elf = (elf_ehdr_t *)vm_binary;

    if( !elf->is_valid() ) {
	fprintf( stderr, "Invalid ELF file: %s\n", path_binary );
	exit( 1 );
    }
 

    // Look for the ELF section that has the addresses for the profile
    // counters.  They are in an unallocated ELF section, so that a running
    // system doesn't load the addresses.  They are in the same order
    // as the counters.
    static const char *sec_name = ".burn_prof_addr";
    elf_shdr_t *sec_burn_addr = elf->get_section( sec_name );
    if( !sec_burn_addr ) {
	fprintf( stderr, "Unable to locate the ELF section %s\n", sec_name );
	exit( 1 );
    }
    const word_t *counter_addrs = (const word_t *)sec_burn_addr->get_file_data(elf);

    if( sec_burn_addr->size != bytes_counters ) {
	fprintf( stderr, "Mismatched size of counters and their addresses\n" );
	exit( 1 );
    }
    word_t cnt_counters = bytes_counters / sizeof(word_t);


    // Locate the ELF symbol table.
    elf_shdr_t *sec_syms = elf->get_sym_section();
    if( !sec_syms ) {
	fprintf( stderr, "Missing symbol section\n" );
	exit( 1 );
    }
    // Locate the ELF string table.
    elf_shdr_t *sec_strings = elf->get_strtab_section();
    if( !sec_strings ) {
	fprintf( stderr, "Missing string ssection\n" );
	exit( 1 );
    }

    // Build a tree of symbols, for quick address lookups.
    elf_sym_t *elf_syms = (elf_sym_t *)sec_syms->get_file_data(elf);
    for( word_t i = 0; i < sec_syms->size/sizeof(elf_sym_t); i++ )
	if( elf_syms[i].is_code() )
	    add_sym( &elf_syms[i] );


    // For each counter, map its address to its function name, and print.
    for( word_t i = 0; i < cnt_counters; i++ ) {
	elf_sym_t *elf_sym = sym_lookup( counter_addrs[i] );
	if( !elf_sym ) {
	    fprintf( stderr, "The address %p has no symbol!\n", 
		    (void *)counter_addrs[i] );
	    exit( 1 );
	}

	printf( "%u\t%s\n", counters[i], elf_sym->get_string(sec_strings, elf));
    }
}

