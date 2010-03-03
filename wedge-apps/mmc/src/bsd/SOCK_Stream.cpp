/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/src/bsd/SOCK_Stream.cpp
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
#include "bsd/SOCK_Stream.h"

#include <sys/socket.h>
#include <unistd.h>

namespace bsd
{

// try to receive exactly len bytes from the connected
// socket to buf
ssize_t
SOCK_Stream::recv_n(void *buf, size_t len,
		    const unsigned long *timeout,
		    size_t *bytes_transferred)
{
    if (len == 0) {
	return 0;
    }
    size_t total_bytes_received = 0;
    ssize_t bytes_received;
    do {
	bytes_received = ::recv(m_handle,
				(static_cast<char *>(buf)) + total_bytes_received,
				len - total_bytes_received, 0);
	if (bytes_received <= 0) {
	    return -1;
	}
	total_bytes_received += bytes_received;
    } while (total_bytes_received != len);
    return len;
}

// try to send exactly len bytes from buf to
// the connected socket
ssize_t
SOCK_Stream::send_n(const void *buf,
		    size_t len,
		    const unsigned long *timeout,
		    size_t *bytes_transferred)
{
    if (len == 0) { return 0; }
    size_t total_bytes_sent = 0;
    ssize_t bytes_sent;
    do {
	bytes_sent = ::send(m_handle,
			    (static_cast<const char *>(buf)) + total_bytes_sent,
			    len - total_bytes_sent, 0);
	if (bytes_sent <= 0) {
	    return -1;
	}
	total_bytes_sent += bytes_sent;
    } while (total_bytes_sent != len);
    return len;
}

// close the stream
int
SOCK_Stream::close(void)
{
    ::close(m_handle);
    return 0;
}

} // namespace bsd
