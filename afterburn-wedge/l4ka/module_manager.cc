/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     l4ka/module_manager.cc
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
 * $Id$
 *                
 ********************************************************************/
#include <l4/types.h>
#include <elfsimple.h>
#include <string.h>

#include INC_WEDGE(resourcemon.h)
#include <console.h>
#include <debug.h>
#include INC_WEDGE(module_manager.h)

module_manager_t module_manager;

// This function takes the raw GRUB module command line, and removes
// the module name from the parameters, and returns the parameters.
const char *module_t::cmdline_options( void )
{
    char *c = this->cmdline;
    // Skip the module name.
    while( *c && !isspace( *c ) )
	c++;

    // Skip white space.
    while( *c && isspace( *c ) )
	c++;

    return c;
}


// Returns true if the raw GRUB module command line has the substring
// specified by the substr parameter.
bool module_t::cmdline_has_substring(  const char *substr )
{
    char *c = (char*) cmdline_options();
    
    if( !*c )
	return false;

    return strstr( c, substr ) != NULL;
}


// This function extracts the size specified by a command line parameter
// of the form "name=val[K|M|G]".  The name parameter is the "name=" 
// string.  The option parameter contains the entire "name=val"
// string.  The terminating characters, K|M|G, will multiply the value
// by a kilobyte, a megabyte, or a gigabyte, respectively.  Zero
// is returned upon any type of error.
L4_Word_t module_t::parse_size_option( 	const char *name, const char *option )
{
    char *next;

    L4_Word_t val = strtoul( option + strlen(name), &next, 0 );

    // Look for size qualifiers.
    if( *next == 'K' ) val = KB(val);
    else if( *next == 'M' ) val = MB(val);
    else if( *next == 'G' ) val = GB(val);
    else
	next--;

    // Make sure that a white space follows.
    next++;
    if( *next && !isspace( *next ) )
	return 0;

    return val;
}


// Thus function scans for a variable on the command line which
// uses memory-style size descriptors.  For example: offset=10G
// The token parameter specifices the parameter name on the command
// line, and must include the = or whatever character separates the 
// parameter name from the value.
L4_Word_t module_t::get_module_param_size( const char *token )
{
    L4_Word_t param_size = 0;
    char *c = (char*) cmdline_options();

    if( !*c )
	return param_size;

    char *token_str = strstr( c, token );
    if( NULL != token_str )
	param_size = parse_size_option( token, token_str );

    return param_size;
}


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

