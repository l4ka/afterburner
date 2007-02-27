/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/include/l4ka/iostream.h
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
 * $Id: iostream.h,v 1.3 2005/04/13 15:47:33 joshua Exp $
 *
 ********************************************************************/
#ifndef __AFTERBURN_WEDGE__INCLUDE__L4KA__HIOSTREAM_H__
#define __AFTERBURN_WEDGE__INCLUDE__L4KA__HIOSTREAM_H__

#include <l4/kdebug.h>
#include <hiostream.h>
#include INC_WEDGE(vcpulocal.h)
#include INC_WEDGE(resourcemon.h)

class hiostream_kdebug_t : public hiostream_driver_t
{
    static const int buf_count = IConsole_max_len;
    static const int max_clients = 4 * CONFIG_NR_VCPUS;
    
    typedef struct 
    {
	int count;
	char buf[buf_count];
    } buffer_t;
    static buffer_t buffer[max_clients];
    static int clients;
  
    int client_base;
    
    static IConsole_handle_t handle;
    static IConsole_content_t content;
    static CORBA_Environment env;
    static bool single_user;
    
    static void flush(int client)
	{
	    env = idl4_default_environment;
	    content.len = buffer[client].count;
	    ASSERT(content.len <= IConsole_max_len);
	    
	    for (word_t i=0; i < content.len; i++)
		content.raw[i] = buffer[client].buf[i];
	   
	    IResourcemon_put_chars(
		resourcemon_shared.cpu[L4_ProcessorNo()].thread_server_tid,
		handle, &content, &env);
		
	    buffer[client].count = 0;
	}

public:
    virtual void init()
	{ 	    
	    this->color = this->background = unknown; 
	    client_base = clients;
	    clients += CONFIG_NR_VCPUS;
	    ASSERT(clients < max_clients);
	    for (int v=0; v <= CONFIG_NR_VCPUS; v++)
		buffer[client_base + v].count = 0;
	    handle = 0;
	    
	}	
    virtual void print_char( char ch )
	{ 
	    int c = client_base + get_vcpu().cpu_id;
	    if (c > 0) single_user = false;
	    ASSERT(c < max_clients);
	    buffer[c].buf[buffer[c].count++] = ch;
	    if (single_user 
		|| buffer[c].count == buf_count  
		|| ch == '\n' 
		|| ch == '\r' )
		flush(c);
	}

    virtual char get_blocking_char()
	{ return L4_KDB_ReadChar_Blocked(); }
};


#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__HIOSTREAM_H__ */
