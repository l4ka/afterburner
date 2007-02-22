#ifndef __INCLUDE__L4KA__MODULE_H__
#define __INCLUDE__L4KA__MODULE_H__

#include <l4/types.h>
#include <resourcemon_idl_client.h>

class module_t : public IResourcemon_shared_module_t
{
public:
    const char *cmdline_options( void );
    bool cmdline_has_substring( const char *substr );
    L4_Word_t get_module_param_size( const char *token );

private:    
    L4_Word_t parse_size_option( const char *name, const char *option );
};


#endif // __INCLUDE__L4KA__MODULE_H__
