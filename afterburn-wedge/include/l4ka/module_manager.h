#ifndef _L4KA__VT__MODULE_MANAGER_H_
#define _L4KA__VT__MODULE_MANAGER_H_

#include <l4/types.h>
#include "resourcemon_idl_client.h"
#include INC_WEDGE(module.h)

class module_manager_t
{
public:
    bool init( void );
    module_t* get_module( L4_Word_t index );
    L4_Word_t get_module_count( void );
    void dump_modules_list( void );

private:
    module_t *modules;
};

extern inline module_manager_t* get_module_manager()
{
    extern module_manager_t module_manager;
    return &module_manager;
}

#endif // _L4KA__VT__MODULE_MANAGER_H_
