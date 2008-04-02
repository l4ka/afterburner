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

#include <resourcemon/freq_acpi.h>

// MANUEL: HACK! copied dev_remap_pgsize from pager.cc
static const L4_Word_t device_remap_pgsize = (1UL << 12);

void acpi_map_rest(word_t phys_addr, word_t size) {
    /* MANUEL: HACK! request the memory for the complete dsdt.
     * There is however no guarantee that the dev_remap_table entries are consecutive!
     * Should probably not use the remap area at all...
     */
    //printf("ACPI remap\n");
    //printf("\taddress %x size %d\n", phys_addr, size);
    word_t remap_start = phys_addr & ~(device_remap_pgsize-1); // start of mapping
    //printf("\tremap_start_phys %x\n", remap_start);
    word_t already_mapped = device_remap_pgsize - (phys_addr - remap_start); // bytes already mapped
    //printf("\talready mapped %d\n", already_mapped);
    word_t still_to_map = size > already_mapped ? size - already_mapped : 0; // bytes still to map
    //printf("\tstill to map %d\n", still_to_map);
    word_t no_mappings = still_to_map / device_remap_pgsize; // number of additional mappings
    no_mappings = (still_to_map % device_remap_pgsize) ? ++no_mappings : no_mappings;
    //printf("\tno mappings %d\n", no_mappings);
    for (u32_t i = 1; i <= no_mappings; i++)
	get_device(phys_addr+i*device_remap_pgsize, 1);
};


/******************************************************
 * DSDT / SSDT class methods
 ******************************************************/

u8_t*
acpi_dsdt_t::get_package(const char name[5], u32_t n) { 
	// returns a pointer to the n-th occurence of name or 0
	if (name[4] != '\0') 
	    L4_KDB_Enter("acpi_dsdt_t::get_package: name not \\0 terminated!");
	
	u32_t occurence = 0;
	
	printf("ACPI get package %C\n", DEBUG_TO_4CHAR(name));
	printf("\toccurence %d of package %C requested, code %x\n",
	       occurence, DEBUG_TO_4CHAR(name), code); 
	
	for (u32_t i = 0; i < header.len - sizeof(acpi_thead_t); i++) { // walk the whole code

	    if (code[i] != name[0]) continue;
		if (strncmp(code + i, name, 4) != 0) continue;
		printf("\tfound %C at %x\n", DEBUG_TO_4CHAR(code + i), code + i);
		if (++occurence == n) return (u8_t*)(code + i);
	}
	return 0;
}
