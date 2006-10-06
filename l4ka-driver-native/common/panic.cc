
#include <l4/kdebug.h>

#include <common/debug.h>

extern "C" NORETURN void panic( void )
{
    while( 1 )
    {
	L4_KDB_Enter( "panic" );
    }
}

