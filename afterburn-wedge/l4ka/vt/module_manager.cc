
#include <l4/types.h>

#include INC_WEDGE(vt/module_manager.h)
#include INC_WEDGE(resourcemon.h)
#include INC_WEDGE(console.h)
#include INC_WEDGE(debug.h)
#include <elfsimple.h>
#include INC_WEDGE(vt/vm.h)
#include INC_WEDGE(vt/string.h)

#include INC_WEDGE(vt/helpers.h)

module_manager_t module_manager;

bool module_manager_t::init( void )
{
    this->modules = (module_t*) resourcemon_shared.modules;
    this->dump_modules_list();

    return true;
}

void module_manager_t::dump_modules_list( void )
{
    for( L4_Word_t i = 0; i < this->get_module_count(); ++i ) {
	con << "Module " << i << " at " << (void *) modules[i].vm_offset
	    << ", size " << (void *) modules[i].size << ",\n  command line: " << modules[i].cmdline
	    << ".\n";
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

