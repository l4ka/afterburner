/*********************************************************************
 *                
 * Copyright (C) 2005-2006,  University of Karlsruhe
 *                
 * File path:     wedge-apps/elfsimple.cc
 * Description:   Simple support for ELF file parsing.
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

#include <stdlib.h>
#include <string.h>

#include "elfsimple.h"

void elf_ehdr_t::get_phys_max_min( word_t vaddr_offset, 
	word_t &max_addr, word_t &min_addr )
{
    word_t image_start = (word_t)this;
    max_addr = 0UL;
    min_addr = ~0UL;

    // Walk the program header table
    for (word_t i = 0; i < this->phnum; i++)
    {
        // Locate the entry in the program header table
        elf_phdr_t* ph = (elf_phdr_t*) 
	    (image_start + this->phoff + this->phentsize * i);
        
        // Is it to be loaded?
        if (ph->type == PT_LOAD)
        {
	    word_t phys_addr = ph->paddr - vaddr_offset;

	    if( phys_addr < min_addr )
		min_addr = phys_addr;

	    word_t size = (ph->msize > ph->fsize) ? ph->msize:ph->fsize;
	    if( (phys_addr+size) > max_addr )
		max_addr = phys_addr + size;
        }
    }
}

elf_shdr_t * elf_ehdr_t::get_section( const char *name )
{
    elf_shdr_t *shdr_table = this->get_shdr_table();
    const char *section_strings = this->get_section_strings();
    if( section_strings == NULL )
	return NULL;

    for( word_t i = 0; i < this->get_num_sections(); i++ ) {
	const char *sec_name = section_strings + shdr_table[i].name;
	if( !strcmp(name, sec_name) )
	    return &shdr_table[i];
    }
    return NULL;
}

elf_shdr_t * elf_ehdr_t::get_sym_section()
{
    elf_shdr_t *shdr_table = this->get_shdr_table();

    for( word_t i = 0; i < this->get_num_sections(); i++ )
	if( shdr_table[i].type == elf_shdr_t::shdr_symtab )
	    return &shdr_table[i];

    return NULL;
}

elf_shdr_t * elf_ehdr_t::get_strtab_section()
{
    elf_shdr_t *shdr_table = this->get_shdr_table();

    for( word_t i = 0; i < this->get_num_sections(); i++ )
    {
	elf_shdr_t &sec = shdr_table[i];
	if( sec.type == elf_shdr_t::shdr_symtab )
	    return (sec.link < this->get_num_sections()) ? 
		&shdr_table[sec.link] : NULL;
    }

    return NULL;
}

word_t elf_ehdr_t::get_file_size()
{
    word_t max = sizeof(elf_ehdr_t);
    word_t i, end;

    end = this->phoff + this->phentsize * this->phnum;
    if( end > max )
	max = end;
    end = this->shoff + this->shentsize * this->shnum;
    if( end > max )
	max = end;
    
    // Find the max program header.
    for( i = 0; i < this->phnum; i++ )
    {
	elf_phdr_t &ph = this->get_phdr_table()[i];
	end = ph.offset + ph.fsize;
	if( end > max )
	    max = end;
    }

    // Find the max section header.
    for( i = 0; i < this->shnum; i++ )
    {
	elf_shdr_t &sh = this->get_shdr_table()[i];
	end = sh.offset + sh.entsize;
	if( end > max )
	    max = end;
    }

    return max;
}

