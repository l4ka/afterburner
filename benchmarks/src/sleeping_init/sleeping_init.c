/* sleeping_init.c
 *
 * This program waits forever.  Its sole purpose is to do nothing.
 * We need it because Linux *must* load a /sbin/init even if we don't want 
 * to do something at user level.
 */

#include <unistd.h>

int main( void )
{
    int fds[2];
    char ch;

    pipe( fds );
    read( fds[0], &ch, sizeof(ch) );
}

