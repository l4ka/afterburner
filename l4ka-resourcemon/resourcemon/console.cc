/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/console.c
 * Description:   Implements a virtual console device for the virtual machines.
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
 ********************************************************************/

#include <resourcemon/resourcemon.h>
#include "resourcemon_idl_server.h"
#include <common/console.h>
#include <common/macros.h>

static hiostream_t con;

IDL4_INLINE void IResourcemon_put_chars_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM,
	const IConsole_handle_t handle ATTR_UNUSED_PARAM,
	const IConsole_content_t *content,
	idl4_server_environment *_env ATTR_UNUSED_PARAM)
{
    word_t len = min(content->len, IConsole_max_len);
	
    for( unsigned i = 0; i < len; i++ )
	con << content->raw[i];
}
IDL4_PUBLISH_IRESOURCEMON_PUT_CHARS(IResourcemon_put_chars_implementation);


IDL4_INLINE void IResourcemon_get_chars_implementation(
	CORBA_Object _caller ATTR_UNUSED_PARAM,
	const IConsole_handle_t handle ATTR_UNUSED_PARAM,
	IConsole_content_t *content,
	idl4_server_environment *_env ATTR_UNUSED_PARAM)
{
    content->len = 0;
}
IDL4_PUBLISH_IRESOURCEMON_GET_CHARS(IResourcemon_get_chars_implementation);


void client_console_init( void )
{
    con.init( hout.get_driver() );
}

