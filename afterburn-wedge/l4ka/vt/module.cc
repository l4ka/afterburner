
#include INC_ARCH(types.h)
#include INC_WEDGE(module.h)
#include INC_WEDGE(vt/string.h)

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
