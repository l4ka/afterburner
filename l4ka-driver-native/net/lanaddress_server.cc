
#include <common/console.h>
#include <common/resourcemon.h>
#include <common/hthread.h>

#include <net/lanaddress_server.h>

static lanaddress_t lanaddress_base;
static u16_t lanaddress_handle = 0;
static bool lanaddress_valid = false;

void lanaddress_set_seed( u8_t *new_addr )
{
    lanaddress_t *new_lanaddress = (lanaddress_t *)new_addr;

    lanaddress_base.align4.lsb = new_lanaddress->align4.lsb; 
    lanaddress_base.align4.msb = new_lanaddress->align4.msb; 
    lanaddress_valid = true;
}

IDL4_INLINE void ILanAddress_generate_lan_address_implementation(
	CORBA_Object _caller,
	lanaddress_t *lanaddress,
	idl4_server_environment *_env)
{
    if( !lanaddress_valid )
    {
	CORBA_exception_set( _env, ex_ILanAddress_insufficient_resources, NULL);
	return;
    }

    // Create a new address, combined from the base, and the handle.
    lanaddress_set_handle( &lanaddress_base, lanaddress_handle );
    lanaddress->align4.lsb = lanaddress_base.align4.lsb;
    lanaddress->align4.msb = lanaddress_base.align4.msb;

    // Move to the next handle.
    lanaddress_handle++;
    if( lanaddress_handle == 0 )
    {
	// wrap around
	lanaddress_valid = 0;
    }
}
IDL4_PUBLISH_ILANADDRESS_GENERATE_LAN_ADDRESS(ILanAddress_generate_lan_address_implementation);


void lanaddress_server_thread( void *p, hthread_t *hthread )
{
    L4_ThreadId_t partner;
    L4_MsgTag_t msgtag;
    idl4_msgbuf_t msgbuf;
    long cnt;
    static void *ILanAddress_vtable[ILANADDRESS_DEFAULT_VTABLE_SIZE] = 
	ILANADDRESS_DEFAULT_VTABLE;

    idl4_msgbuf_init( &msgbuf );

    while( 1 )
    {
	partner = L4_nilthread;
	msgtag.raw = 0;
	cnt = 0;

	while( 1 )
	{
	    idl4_msgbuf_sync( &msgbuf );
	    idl4_reply_and_wait( &partner, &msgtag, &msgbuf, &cnt );
	    if( idl4_is_error( &msgtag ) )
	     	break;

	    idl4_process_request( &partner, &msgtag, &msgbuf, &cnt, 
		    ILanAddress_vtable[
		        idl4_get_function_id(&msgtag) & ILANADDRESS_FID_MASK] );
	}
    }
}

static const word_t stack_size = KB(4);
static u8_t thread_stack[stack_size];

L4_ThreadId_t lanaddress_init( void )
    // The initial entry point for the LanAddress allocator service.
{
    hthread_t *hthread = get_hthread_manager()->create_thread(
	    (L4_Word_t)thread_stack, stack_size, get_max_prio(),
	    lanaddress_server_thread, L4_Myself(), get_thread_server_tid() );
    if( hthread == NULL )
	return L4_nilthread;

    hthread->start();

    if( !resourcemon_register_service(UUID_ILanAddress, hthread->get_global_tid()) )
	printf( "Error registering the lanaddress allocator with the locator.\n" );

    return hthread->get_global_tid();
}

