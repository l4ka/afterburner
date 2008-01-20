/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/amd64/bind.cc
 * Description:   Bind to the high-level hooks in a guest OS.
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

#include INC_WEDGE(backend.h)
#include <console.h>
#include INC_WEDGE(vcpulocal.h)

#include <bind.h>

void * (*afterburn_thread_get_handle)(void) = NULL;
void (*afterburn_thread_assign_handle)(void *handle) = NULL;
#if defined(CONFIG_VSMP)
word_t (*afterburn_cpu_get_startup_ip)(word_t apic_id) = NULL;
#endif

guest_uaccess_fault_t *guest_uaccess_fault = NULL;
word_t *guest_burn_prof_counters_start = NULL;
word_t *guest_burn_prof_counters_end = NULL;

#if !defined(CONFIG_L4KA_VT)

struct bind_from_guest_t {
    bind_from_guest_e type;
    void **dangler;
};

struct bind_to_guest_t {
    bind_to_guest_e type;
    void *func;
    bool exported;
};

bind_from_guest_t bind_from_guest[] = {
#if defined(CONFIG_WEDGE_L4KA)
    { import_thread_get_handle, (void **)&afterburn_thread_get_handle },
    { import_thread_set_handle, (void **)&afterburn_thread_assign_handle },
#endif
    { import_uaccess_fault_handler, (void **)&guest_uaccess_fault },
    { import_burn_prof_counters_start, (void **)&guest_burn_prof_counters_start },
    { import_burn_prof_counters_end, (void **)&guest_burn_prof_counters_end },
#if defined(CONFIG_VSMP)
    { import_cpu_get_startup_ip, (void **)&afterburn_cpu_get_startup_ip },
#endif
    
};
static const word_t bind_from_guest_cnt = 
    sizeof(bind_from_guest)/sizeof(bind_from_guest[0]);


bind_to_guest_t bind_to_guest[] = {
#if (defined(CONFIG_WEDGE_L4KA) && !defined(CONFIG_L4KA_VMEXT)) || defined(CONFIG_WEDGE_LINUX)
    { import_exit_hook, (void *)backend_exit_hook, false },
    { import_signal_hook, (void *)backend_signal_hook, false },
#endif
#if defined(CONFIG_GUEST_PTE_HOOK)
    { import_set_pte_hook, (void *)backend_set_pte_hook, false },
    { import_pte_test_and_clear_hook, (void *)backend_pte_test_and_clear_hook, false },
    { import_pte_get_and_clear_hook, (void *)backend_pte_get_and_clear_hook, false },
    { import_read_pte_hook, (void *)backend_read_pte_hook, false },
#endif
#if (defined(CONFIG_GUEST_PTE_HOOK) && CONFIG_WEDGE_XEN) || (CONFIG_L4KA_VMEXT)
    { import_free_pgd_hook, (void *)backend_free_pgd_hook, false },
#endif
#if defined(CONFIG_GUEST_UACCESS_HOOK)
    { import_get_user_hook, (void *)backend_get_user_hook, false },
    { import_put_user_hook, (void *)backend_put_user_hook, false },
    { import_copy_from_user_hook, (void *)backend_copy_from_user_hook, false },
    { import_copy_to_user_hook, (void *)backend_copy_to_user_hook, false },
    { import_clear_user_hook, (void *)backend_clear_user_hook, false },
    { import_strnlen_user_hook, (void *)backend_strnlen_user_hook, false },
    { import_strncpy_from_user_hook, (void *)backend_strncpy_from_user_hook, false },
#endif
#if defined(CONFIG_GUEST_DMA_HOOK)
    { import_dma_coherent_check, (void *)backend_dma_coherent_check, false },
    { import_phys_to_dma_hook, (void *)backend_phys_to_dma_hook, false },
    { import_dma_to_phys_hook, (void *)backend_dma_to_phys_hook, false },
#endif
#if defined(CONFIG_GUEST_MODULE_HOOK)
    { import_module_rewrite_hook, (void *)backend_module_rewrite_hook, false },
#endif
};
static const word_t bind_to_guest_cnt = 
    sizeof(bind_to_guest)/sizeof(bind_to_guest[0]);


bool
arch_bind_from_guest( elf_bind_t *guest_exports, word_t count )
{
    ASSERT( sizeof(elf_bind_t) == 2*sizeof(word_t) );
    word_t i, j;
    
    printf( "Required from guest: %d total from guest %t\n", 
	    bind_from_guest_cnt, count);

    // Walk the list of exports from the guest OS.
    for( i = 0; i < count; i++ ) {
	bool found = false;
	// Walk the import table, to find the export's matching import.
	for( j = 0; !found && (j < bind_from_guest_cnt); j++ ) 
	{
	    if( bind_from_guest[j].type != (bind_from_guest_e)guest_exports[i].type ) 
		continue;
	    if( *bind_from_guest[j].dangler )
    		printf( "Multiple guest exports of type %d\n", guest_exports[i].type);
	    *bind_from_guest[j].dangler = (void *)guest_exports[i].location;
	    found = true;
	}

//	if( !found )
//	    printf( "Unused guest export of type " 
//		<< guest_exports[i].type << '\n';
    }

    // Determine whether we had any unsatisfied symbols.
    bool complete = true;
    for( j = 0; j < bind_from_guest_cnt; j++ )
	if( !*bind_from_guest[j].dangler ) {
	    printf( "Unsatisfied import from the guest OS, type %d\n", guest_exports[j].type);
	    complete = false;
	}

    return complete;
}

bool
arch_bind_to_guest( elf_bind_t *guest_imports, word_t count )
{
    ASSERT( sizeof(elf_bind_t) == 2*sizeof(word_t) );
    word_t i, j;
    
    printf( "Required exports to guest guest: %d total from guest %t\n", 
	    bind_to_guest_cnt, count);

    // Walk the list of imports from the guest OS.
    for( i = 0; i < count; i++ ) {
	bool found = false;
	// Walk the export table, to find the import's matching import.
	for( j = 0; !found && (j < bind_to_guest_cnt); j++ ) 
	{
	    if( bind_to_guest[j].type != (bind_to_guest_e)guest_imports[i].type ) 
		continue;
	    bind_to_guest[j].exported = true;
	    void **guest_location = (void **)(guest_imports[i].location - get_vcpu().get_kernel_vaddr());
	    *guest_location = bind_to_guest[j].func;
	    found = true;
	}

//	if( !found )
//	    printf( "Unused export of type " << guest_imports[i].type << '\n';
    }

    // Determine whether we had any unsatisfied symbols.
    bool complete = true;
    for( j = 0; j < bind_to_guest_cnt; j++ )
	if( !bind_to_guest[j].exported ) {
	    printf( "Unsatisfied import to the guest OS, type %d\n", bind_to_guest[j].type);
	    complete = false;
	}

    return complete;
}
#endif /* !defined(CONFIG_L4KA_VT) */

