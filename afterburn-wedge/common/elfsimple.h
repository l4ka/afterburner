/*********************************************************************
 *                
 * Copyright (C) 2005,  University of Karlsruhe
 *                
 * File path:     afterburn-wedge/elf.h
 * Description:   Elf 32 and 64-bit binary types.
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
 * $Id: elfsimple.h,v 1.7 2005/09/02 20:07:13 joshua Exp $
 *                
 ********************************************************************/
#ifndef __ELF_H__
#define __ELF_H__

/*
 *  Assumption:
 *
 *  We load only files for the platform we're being built for.
 */

#include INC_ARCH(types.h)

class elf_ehdr_t;

class elf_reloc_t
{
public:
    enum reloc32_e {reloc_none=0, reloc_32=1, reloc_pc32=2};

    word_t offset;
    struct {
#if CONFIG_BITWIDTH == 64
	word_t type : 32;
	word_t sym : 32;
#else
	word_t type : 8;
	word_t sym : 24;
#endif
    };
};

class elf_sym_t
{
public:
    u32_t name;
#if CONFIG_BITWIDTH == 64
    u8_t   info;
    u8_t   other;
    u16_t  shndx;
#endif
    word_t value;
    word_t size;
#if CONFIG_BITWIDTH == 32
    u8_t   info;
    u8_t   other;
    u16_t  shndx;
#endif
};

class elf_phdr_t
{
public:
    u32_t  type;
#if CONFIG_BITWIDTH == 64
    u32_t  flags;
#endif
    word_t offset;
    word_t vaddr;
    word_t paddr;
    word_t fsize;
    word_t msize;
#if CONFIG_BITWIDTH == 32
    u32_t   flags;
#endif
    word_t  align;
};

class elf_shdr_t
{
public:
    enum shdr_type_e {
	shdr_null=0, shdr_progbits=1, shdr_symtab=2,
	shdr_strtab=3, shdr_rela=4, shdr_note=7, shdr_rel=9,
	shdr_nobits=8
    };
    enum shdr_index_e {
	idx_undef=0, idx_abs = 0xfff1
    };

    u32_t  name;
    u32_t  type;
    word_t flags;
    word_t addr;
    word_t offset;
    word_t size;
    u32_t  link;
    u32_t  info;
    word_t addralign;
    word_t entsize;

    void *get_file_data( elf_ehdr_t *ehdr )
	{ return (void *)(offset + (word_t)ehdr); }
};

enum phdr_type_e 
{
    PT_NULL =   0,      /* unused program segment */
    PT_LOAD =   1       /* Loadable program segment */
};


class elf_ehdr_t 
{
public:
    unsigned char ident[16];
    u16_t  type;
    u16_t  machine;
    u32_t  version;
    word_t entry;          // Program start address
    word_t phoff;          // File offset of program header table
    word_t shoff;
    u32_t  flags;
    u16_t  ehsize;         // Size of this ELF header
    u16_t  phentsize;      // Size of a program header
    u16_t  phnum;          // Number of program headers
    u16_t  shentsize;
    u16_t  shnum;
    u16_t  shstrndx;

    inline bool is_valid()
	{
	    return (this->ident[0] == 0x7f) && (this->ident[1] == 'E')
	      	&& (this->ident[2] == 'L') && (this->ident[3] == 'F')
		&& (this->type == 2);
	}

    inline word_t get_num_sections()
	{ return shnum; }

    inline elf_phdr_t *get_phdr_table()
	{ return (elf_phdr_t *)(word_t(this) + this->phoff); }
    inline elf_shdr_t *get_shdr_table()
	{ return (elf_shdr_t *)(word_t(this) + this->shoff); }

    const char *get_section_strings()
	{
	    word_t str_start = get_shdr_table()[shstrndx].offset;
	    if( str_start == 0 )
		return NULL;
    	    return (const char *)(word_t(this) + str_start);
	}

    elf_shdr_t *get_section(const char *name);
    elf_shdr_t *get_sym_section();
    elf_shdr_t *get_strtab_section();
    bool load_phys( word_t vaddr_offset );
    void get_phys_max_min( word_t vaddr_offset, 
	    word_t &max_addr, word_t &min_addr );
    word_t get_file_size();
};

INLINE elf_ehdr_t * elf_is_valid( word_t image_start )
{
    elf_ehdr_t * eh = (elf_ehdr_t *)image_start;
    if( eh->is_valid() )
	return eh;
    else
	return NULL;
}

#endif	/* __ELF_H__ */
