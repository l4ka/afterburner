/*********************************************************************
 *                
 * Copyright (C) 2005,  University of Karlsruhe
 *                
 * File path:     elfrewrite.cc
 * Description:   Rewrite the instructions of an ELF file.
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
 * $Id: elfrewrite.cc,v 1.12 2006/03/23 00:39:23 joshua Exp $
 *                
 ********************************************************************/

#include INC_WEDGE(console.h)
#include INC_WEDGE(debug.h)
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(backend.h)

#include INC_ARCH(sync.h)
#include INC_ARCH(vmi.h)

#include <elfsimple.h>
#include <burn_symbols.h>
#include <bind.h>
#include <profile.h>

#include <device/dp83820.h>

static const bool debug_reloc = 0;
static const bool debug_resolve = 0;

#if defined(CONFIG_INSTR_PROFILE)
instr_group_t instr_group_pte_read  = { "pte read" };
instr_group_t instr_group_pte_write = { "pte write" };
instr_group_t instr_group_pgd_write = { "pgd write" };
instr_group_t instr_group_pmd_write = { "pmd write" };
instr_group_t instr_group_pte_test_clear = { "pte test clear" };
#if defined(CONFIG_DEVICE_DP83820)
instr_group_t instr_group_dp83820 = { "dp83820" };
#endif
#endif

// TODO: handle corrupt ELF headers to avoid security holes.
// - Check for section sizes that are too big for their sections.
// - Check for section sizes that are too small.
// - Check for references between sections that are no within section bounds.
#warning JTL: Corrupt module ELF headers are not handled.  Major security hole.

bool frontend_elf_relocate( 
	elf_ehdr_t *elf, elf_shdr_t *reloc_sec, elf_shdr_t *sym_sec )
{
    if( reloc_sec->type != elf_shdr_t::shdr_rel ) {
	con << "Unimplemented relocation type.\n";
	return false;
    }

    elf_reloc_t *relocs = (elf_reloc_t *)reloc_sec->get_file_data( elf );
    word_t reloc_cnt = reloc_sec->size / sizeof(elf_reloc_t);
    elf_sym_t *syms = (elf_sym_t *)sym_sec->get_file_data( elf );

    elf_shdr_t *target_sec = &elf->get_shdr_table()[ reloc_sec->info ];
    word_t target_base = word_t(target_sec->get_file_data(elf));

    if( debug_reloc )
	con << "target sec " << reloc_sec->info << '\n';

    for( word_t i = 0; i < reloc_cnt; i++ )
    {
	// Where to make the change.
	word_t *location = (word_t *)(target_base + relocs[i].offset);
	// The symbol being referred to.
	elf_sym_t *sym = &syms[relocs[i].sym];
	elf_shdr_t *sym_target_sec = &elf->get_shdr_table()[ sym->shndx ];

	if( debug_reloc )
	    con << "relocation at " << (void *)location 
		<< ", is " << (void *)*location
		<< ", sec index " << sym->shndx;

	switch( relocs[i].type ) {
	    case elf_reloc_t::reloc_32 :
		*location += (word_t)sym_target_sec->get_file_data(elf);
		break;
	    case elf_reloc_t::reloc_pc32 :
		*location += (word_t)sym_target_sec->get_file_data(elf) - word_t(location);
		break;
	    default:
		con << "Unsupported ELF relocation type.\n";
		return false;
	}

	if( debug_reloc )
	    con << ", new " << (void *)*location << '\n';
    }

    return true;
}

bool frontend_elf_resolve_symbols( 
	elf_ehdr_t *elf, elf_shdr_t *wedge_sec, elf_shdr_t *sym_sec )
{
    // The loading module has an ELF section with a header to help 
    // negotiate the dynamic linking.  First validate the header, and
    // ensure that the module is compatible with the current version of the
    // wedge.
    burn_wedge_header_t *wedge_header = (burn_wedge_header_t *)
	wedge_sec->get_file_data(elf);
    if( wedge_sec->size < sizeof(burn_wedge_header_t) ||
	    wedge_header->version < burn_wedge_header_t::current_version )
    {
	con << "Error: attempt to dynamically link against an old or "
	    "corrupt module.\n";
	return false;
    }

    // Locate the strings ELF section.
    elf_shdr_t *strtab_sec = elf->get_strtab_section();
    if( !strtab_sec ) {
	con << "Error: corrupt ELF headers.\n";
	return false;
    }
    const char *strtab = (const char *)strtab_sec->get_file_data(elf);

    // Now walk the symbols in the loading module, and resolve them against
    // exported burn symbols in the wedge.
    elf_sym_t *syms = (elf_sym_t *)sym_sec->get_file_data(elf);
    word_t sym_cnt = sym_sec->size / sizeof(elf_sym_t);

    for( word_t i = 0; i < sym_cnt; i++ )
    {
	if( syms[i].shndx != elf_shdr_t::idx_undef )
	    continue;

	const char *name = strtab + syms[i].name;
	burn_symbol_t *burn_sym = get_burn_symbols().lookup_symbol( name );
	if( burn_sym ) {
	    atomic_inc( &burn_sym->usage_count );
	    syms[i].value = word_t(burn_sym->ptr);
	    // Prevent Linux from trying to re-resolve this symbol.
	    syms[i].shndx = elf_shdr_t::idx_abs;
	}

	if( debug_resolve )
	    if( burn_sym )
		con << "Resolved symbol " << name 
		    << ", usage count " << burn_sym->usage_count
		    << ", address " << (void *)burn_sym->ptr << '\n';
	    else
		con << "Unresolved symbol: " << name << '\n';

    }

    return true;
}


bool frontend_vmi_rewrite( elf_ehdr_t *elf, word_t vaddr_offset )
{
    elf_shdr_t *elf_annotations = elf->get_section( VMI_ANNOTATION_ELF_SECTION);
    if( !elf_annotations )
	return false;
    return true;

    // VMI uses allocated ELF sections, and thus they are remapped along
    // with the rest of the guest kernel.
    vmi_annotation_t *annotations = (vmi_annotation_t *)
	(elf_annotations->addr - vaddr_offset);
    word_t count = elf_annotations->size / sizeof(vmi_annotation_t);

    ASSERT( count > 0 );
    if( !vmi_apply_patchups(annotations, count, vaddr_offset) )
	return false;

    UNIMPLEMENTED();

    return true;
}

bool frontend_elf_rewrite( elf_ehdr_t *elf, word_t vaddr_offset, bool module )
{
    patchup_info_t *patchups;
    word_t count;

    ASSERT( elf );

    elf_shdr_t *syms = elf->get_sym_section();

    // If a module wants to link against the afterburner, then it
    // will have a .afterburn_wedge_module ELF section.
    elf_shdr_t *wedge_sec = elf->get_section(".afterburn_wedge_module");
    if( wedge_sec && !frontend_elf_resolve_symbols(elf, wedge_sec, syms) )
	return false;

    // Patchup the binary.
    elf_shdr_t *reloc = elf->get_section(".rel.afterburn");
    if( reloc && !frontend_elf_relocate(elf, reloc, syms) )
	return false;
    elf_shdr_t *elf_patchup = elf->get_section(".afterburn");
    if( !elf_patchup && !module ) {
#ifdef CONFIG_VMI_SUPPORT
	return frontend_vmi_rewrite(elf, vaddr_offset);
#else
	con << "Missing patchup section.\n";
	return false;
#endif
    }
    if( elf_patchup ) {
	patchups = (patchup_info_t *)elf_patchup->get_file_data(elf);
	count = elf_patchup->size / sizeof(patchup_info_t);
	if( !arch_apply_patchups(patchups, count, vaddr_offset) ) {
	    con << "Failed to apply patchups.\n";
	    return false;
	}
    }

    // Do device patchups.
#if defined(CONFIG_DEVICE_DP83820)
    reloc = elf->get_section(".rel.afterburn.dp83820");
    if( reloc && !frontend_elf_relocate(elf, reloc, syms) )
	return false;
    elf_patchup = elf->get_section(".afterburn.dp83820");
    if( elf_patchup ) {
	patchups = (patchup_info_t *)elf_patchup->get_file_data(elf);
	count = elf_patchup->size / sizeof(patchup_info_t);
	if( !arch_apply_device_patchups(patchups, count, vaddr_offset,
		    (word_t)dp83820_read_patch, 
		    (word_t)dp83820_write_patch, 0) )
	{
	    con << "Failed to apply the dp83820 patchupes.\n";
	    return false;
	}
	ON_INSTR_PROFILE(instr_profile_add_patchups( patchups, count, 
    		    &instr_group_dp83820 ));
    }
#endif
#if defined(CONFIG_DEVICE_APIC)
    reloc = elf->get_section(".rel.afterburn.ioapic");
    if( reloc && !frontend_elf_relocate(elf, reloc, syms) )
	return false;
    elf_patchup = elf->get_section(".afterburn.ioapic");
    if( elf_patchup ) {
	patchups = (patchup_info_t *)elf_patchup->get_file_data(elf);
	count = elf_patchup->size / sizeof(patchup_info_t);
	if( !arch_apply_device_patchups(patchups, count, vaddr_offset,
			(word_t)ioapic_read_patch, 
			(word_t)ioapic_write_patch, 
			(word_t)0))
	    return false;
	
	ON_INSTR_PROFILE(instr_profile_add_patchups( patchups, count, 
			&instr_group_ioapic ));
    }

    reloc = elf->get_section(".rel.afterburn.lapic");
    if( reloc && !frontend_elf_relocate(elf, reloc, syms) )
	return false;
    elf_patchup = elf->get_section(".afterburn.lapic");
    
    if( elf_patchup ) {
	patchups = (patchup_info_t *)elf_patchup->get_file_data(elf);
	count = elf_patchup->size / sizeof(patchup_info_t);
	if( !arch_apply_device_patchups(patchups, count, vaddr_offset,
			(word_t)lapic_read_patch, 
			(word_t)lapic_write_patch, 
			(word_t)lapic_xchg_patch) )
	    return false;
	ON_INSTR_PROFILE(instr_profile_add_patchups( patchups, count, 
			&instr_group_lapic ));
    }
    
#endif

    reloc = elf->get_section(".rel.afterburn.pte_read");
    if( reloc && !frontend_elf_relocate(elf, reloc, syms) )
	return false;
    elf_patchup = elf->get_section(".afterburn.pte_read");
    if( elf_patchup ) {
	patchups = (patchup_info_t *)elf_patchup->get_file_data(elf);
	count = elf_patchup->size / sizeof(patchup_info_t);
	if( !arch_apply_device_patchups(patchups, count, vaddr_offset,
		    (word_t)backend_pgd_read_patch, 0, 
		    (word_t)backend_pte_xchg_patch) )
	{
	    con << "Failed to apply the PTE read patchups.\n";
	    return false;
	}
	ON_INSTR_PROFILE(instr_profile_add_patchups( patchups, count, 
    		    &instr_group_pte_read ));
    }

    reloc = elf->get_section(".rel.afterburn.pgd_read");
    if( reloc && !frontend_elf_relocate(elf, reloc, syms) )
	return false;
    elf_patchup = elf->get_section(".afterburn.pgd_read");
    if( elf_patchup ) {
	patchups = (patchup_info_t *)elf_patchup->get_file_data(elf);
	count = elf_patchup->size / sizeof(patchup_info_t);
	if( !arch_apply_device_patchups(patchups, count, vaddr_offset,
		    (word_t)backend_pgd_read_patch, 0, 
		    (word_t)backend_pte_xchg_patch) )
	{
	    con << "Failed to apply PGD read patchups.\n";
	    return false;
	}
	ON_INSTR_PROFILE(instr_profile_add_patchups( patchups, count, 
		    &instr_group_pte_read ));
    }


    reloc = elf->get_section(".rel.afterburn.pte_set");
    if( reloc && !frontend_elf_relocate(elf, reloc, syms) )
	return false;
    elf_patchup = elf->get_section(".afterburn.pte_set");
    if( elf_patchup ) {
	patchups = (patchup_info_t *)elf_patchup->get_file_data(elf);
	count = elf_patchup->size / sizeof(patchup_info_t);
	if( !arch_apply_device_patchups(patchups, count, vaddr_offset,
		    0, (word_t)backend_pte_write_patch, 0,
		    (word_t)backend_pte_or_patch,
		    (word_t)backend_pte_and_patch) )
	{
	    con << "Failed to apply PTE set patchups.\n";
	    return false;
	}
	ON_INSTR_PROFILE(instr_profile_add_patchups( patchups, count, 
		    &instr_group_pte_write ));
    }
#if !defined(CONFIG_GUEST_PTE_HOOK)
    else if( !module )
	PANIC( "The guest kernel is missing PTE annotations." );
#endif

    reloc = elf->get_section(".rel.afterburn.pmd_set");
    if( reloc && !frontend_elf_relocate(elf, reloc, syms) )
	return false;
    elf_patchup = elf->get_section(".afterburn.pmd_set");
    if( elf_patchup ) {
	patchups = (patchup_info_t *)elf_patchup->get_file_data(elf);
	count = elf_patchup->size / sizeof(patchup_info_t);
	if( !arch_apply_device_patchups(patchups, count, vaddr_offset,
		    0, (word_t)backend_pgd_write_patch, 0) )
	{
	    con << "Failed to apply PMD set patchups.\n";
	    return false;
	}
	ON_INSTR_PROFILE(instr_profile_add_patchups( patchups, count, 
		    &instr_group_pmd_write ));
    }

    reloc = elf->get_section(".rel.afterburn.pgd_set");
    if( reloc && !frontend_elf_relocate(elf, reloc, syms) )
	return false;
    elf_patchup = elf->get_section(".afterburn.pgd_set");
    if( elf_patchup ) {
	patchups = (patchup_info_t *)elf_patchup->get_file_data(elf);
	count = elf_patchup->size / sizeof(patchup_info_t);
	if( !arch_apply_device_patchups(patchups, count, vaddr_offset,
		    0, (word_t)backend_pgd_write_patch, 0) )
	{
	    con << "Failed to apply PGD set patchups.\n";
	    return false;
	}
	ON_INSTR_PROFILE(instr_profile_add_patchups( patchups, count, 
		    &instr_group_pgd_write ));
    }

    reloc = elf->get_section(".rel.afterburn.pte_test_clear");
    if( reloc && !frontend_elf_relocate(elf, reloc, syms) )
	return false;
    elf_patchup = elf->get_section(".afterburn.pte_test_clear");
    if( elf_patchup ) {
	patchups = (patchup_info_t *)elf_patchup->get_file_data(elf);
	count = elf_patchup->size / sizeof(patchup_info_t);
	if( !arch_apply_bitop_patchups(patchups, count, vaddr_offset,
		    (word_t)backend_pte_test_clear_patch, 0) )
	{
	    con << "Failed to apply PTE test-clear patchups.\n";
	    return false;
	}
	ON_INSTR_PROFILE(instr_profile_add_patchups( patchups, count, 
		    &instr_group_pte_test_clear ));
    }


    // Bind stuff to the guest OS.
    elf_shdr_t *elf_imports = elf->get_section(".afterburn.imports");
    if( elf_imports ) {
	elf_bind_t *imports = (elf_bind_t *)elf_imports->get_file_data(elf);
	count = elf_imports->size / sizeof(elf_bind_t);
	if( !arch_bind_to_guest(imports, count) ) {
	    con << "Failed to bind to guest OS.\n";
	    return false;
	}
    }

    // Bind stuff from the guest OS.  Don't do this for modules.  We only
    // import hooks from the guest kernel.
    if( !module )
    {
       	elf_shdr_t *elf_exports = elf->get_section(".afterburn.exports");
	elf_bind_t *exports = NULL;
	if( elf_exports ) {
	    exports = (elf_bind_t *)elf_exports->get_file_data(elf);
	    count = elf_exports->size / sizeof(elf_bind_t);
	}
	else 
	    count = 0;
	if( !arch_bind_from_guest(exports, count) ) {
	    con << "Failed to bind from guest OS.\n";
	    return false;
	}
    }

    ON_INSTR_PROFILE(instr_profile_sort());

    return true;
}

