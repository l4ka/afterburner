
#include <l4/kdebug.h>
#include <l4/ipc.h>

#include <common/hthread.h>
#include <common/resourcemon.h>
#include <common/console.h>

static NORETURN
void master_loop()
{
    L4_ThreadId_t tid = L4_nilthread;
    for (;;)
    {
	L4_MsgTag_t tag = L4_ReplyWait( tid, &tid );

	if( L4_IpcFailed(tag) ) {
	    tid = L4_nilthread;
	    continue;
	}

	printf( "Unhandled message %p from TID %p\n",
		(void *)tag.raw, (void *)tid.raw );
	L4_KDB_Enter("unhandled message");
	tid = L4_nilthread;
    }
}

void drivers_main()
{
    console_init( kdebug_putc, "\e[1m\e[33mdrivers:\e[0m " );
    printf( "L4Ka Native Drivers Project\n" );

    get_hthread_manager()->init( resourcemon_shared.thread_space_start,
	    resourcemon_shared.thread_space_len );

#if defined(cfg_net)
    extern L4_ThreadId_t net_init();
    net_init();
    extern L4_ThreadId_t lanaddress_init();
    lanaddress_init();
#endif

    if( !resourcemon_continue_init() )
	printf( "Failed to continue with system initialization.\n" );

    master_loop();
}

