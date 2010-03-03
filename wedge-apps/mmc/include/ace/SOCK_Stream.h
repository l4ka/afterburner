/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/include/ace/SOCK_Stream.h
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
#ifndef SOCK_STREAM_H_
#define SOCK_STREAM_H_

#include <cstddef>
#include <sys/types.h>

/**
 * @class ACE_SOCK_Stream
 * 
 * @brief stream socket communication
 * 
 */
class ACE_SOCK_Stream
{
public:
	ACE_SOCK_Stream(void) : m_handle(-1) {}
	~ACE_SOCK_Stream(void) {}
	
	// try to receive exactly len bytes from the connected
	// socket to buf
	ssize_t recv_n(void *buf,
				size_t len,
				const unsigned long *timeout = 0,
				size_t *bytes_transferred = 0);
	
	// try to send exactly len bytes from buf to
	// the connected socket
	ssize_t send_n(const void *buf,
				size_t len,
				const unsigned long *timeout = 0,
				size_t *bytes_transferred = 0);
	
	int close(void);
	void setHandle(int handle) { m_handle = handle; }
	
private:
	int m_handle;
};	

#endif /*SOCK_STREAM_H_*/
