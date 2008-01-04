#include <l4/types.h>
#include <elfsimple.h>
#include <string.h>

#include INC_WEDGE(resourcemon.h)
#include <console.h>
#include <debug.h>
#include INC_WEDGE(vm.h)
#include INC_WEDGE(module_manager.h)

module_manager_t module_manager;

bool module_manager_t::init( void )
{
    this->modules = (module_t*) resourcemon_shared.modules;
    //this->dump_modules_list();

    return true;
}

void module_manager_t::dump_modules_list( void )
{
    for( L4_Word_t i = 0; i < this->get_module_count(); ++i ) {
	printf( "Module %d at %x size %08d\ncommand line %s\n",modules[i].vm_offset,
		modules[i].size, modules[i].cmdline);
    }
}

L4_Word_t module_manager_t::get_module_count( void )
{
    return resourcemon_shared.module_count;
}

module_t* module_manager_t::get_module( L4_Word_t index )
{
    if( index < 0 || index >= this->get_module_count() ) {
	return NULL;
    }
    
    return &this->modules[index];
}

