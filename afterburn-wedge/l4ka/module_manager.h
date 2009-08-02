/*********************************************************************
 *                
 * Copyright (C) 2007-2008,  Karlsruhe University
 *                
 * File path:     l4ka/module_manager.h
 * Description:   
 *                
 * @LICENSE@
 *                
 * $Id:$
 *                
 ********************************************************************/
#ifndef __L4KA__MODULE_MANAGER_H__
#define __L4KA__MODULE_MANAGER_H__

#include <l4/types.h>
#include "resourcemon_idl_client.h"

class module_t : public IResourcemon_shared_module_t
{
public:
    const char *cmdline_options( void );
    bool cmdline_has_substring( const char *substr );
    L4_Word_t get_module_param_size( const char *token );

private:    
    L4_Word_t parse_size_option( const char *name, const char *option );
};

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


#endif /* !__L4KA__MODULE_MANAGER_H__ */
