/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4ka/iostream.h
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
#include INC_WEDGE(vcpu.h)
#include INC_WEDGE(sync.h)
#include INC_WEDGE(resourcemon.h)

class hiostream_kdebug_t : public hiostream_driver_t
{
   
    static const int buf_count = IConsole_max_len;
    typedef struct 
    {
	bool auto_commit;
	int  count;
	char buf[buf_count];
	
    } buffer_t;
    static const word_t max_clients = 4 * CONFIG_NR_VCPUS;
    static bool initialized;
    static buffer_t buffer[max_clients];
    static word_t clients;
    static cpu_lock_t lock;
    static IConsole_handle_t handle;
    static IConsole_content_t content;
    static CORBA_Environment env;

    word_t client_base;
    
public:

    void commit(word_t client=max_clients)
	{
	    env = idl4_default_environment;
	    
	    if (client == max_clients)
		client = client_base + get_vcpu().cpu_id;
	    
	    ASSERT(client < max_clients);
	    if (buffer[client].count == 0)
		return;
	    
	    lock.lock();
	    
	    content.len = buffer[client].count;
	    ASSERT(content.len <= IConsole_max_len);

	    for (word_t i=0; i < content.len; i++)
		content.raw[i] = buffer[client].buf[i];
	    
	    L4_Word_t saved_mrs[64];
	    L4_StoreMRs (0, 64, saved_mrs);
	    IResourcemon_put_chars(resourcemon_shared.thread_server_tid, handle, &content, &env);
	    L4_LoadMRs (0, 64, saved_mrs);

	    buffer[client].count = 0;
	    
	    lock.unlock();
	    
	}

    virtual void init(bool auto_commit=true)
	{ 	
	    if (!initialized) 
	    {
		this->color = this->background = unknown; 
		handle = 0;
		lock.init();
	    }
	    initialized = true;
	    
	    client_base = clients;
	    clients += vcpu_t::nr_vcpus;
	    ASSERT(clients < max_clients);
	    for (word_t v=0; v <= vcpu_t::nr_vcpus; v++)
	    {
		buffer[client_base + v].count = 0;
		buffer[client_base + v].auto_commit = auto_commit;
	    }
	    

	}	
    virtual void print_char( char ch )
	{ 
	    word_t client = client_base + get_vcpu().cpu_id;
	    ASSERT(client < max_clients);
	    
	    buffer[client].buf[buffer[client].count++] = ch;
	    if (buffer[client].count == buf_count 
		|| (buffer[client].auto_commit && (ch == '\n' || ch == '\r')))
		commit(client);
	}

    virtual char get_blocking_char()
	{ return L4_KDB_ReadChar_Blocked(); }
};


#endif	/* __AFTERBURN_WEDGE__INCLUDE__L4KA__HIOSTREAM_H__ */
