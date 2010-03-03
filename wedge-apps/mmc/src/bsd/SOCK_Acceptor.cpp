/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/src/bsd/SOCK_Acceptor.cpp
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
#include "bsd/SOCK_Acceptor.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

namespace bsd
{

SOCK_Acceptor::SOCK_Acceptor()
{
}

SOCK_Acceptor::~SOCK_Acceptor(void) throw()
{
}

void
SOCK_Acceptor::init(unsigned short port)
throw (network::NetworkException&)
{
	// create server socket, bind and listen
  	if ((m_serverSocketHandle = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      throw network::NetworkException(strerror(errno));
    }
	int flag = 1;
	if (setsockopt(m_serverSocketHandle, SOL_SOCKET, SO_REUSEADDR, (void *)&flag,
                 	sizeof(int)) < 0) {
      	close();
      	throw network::NetworkException(strerror(errno));
  	}
	struct sockaddr_in servaddr;
  	memset(&servaddr, 0x00, sizeof(servaddr));
  	servaddr.sin_family = AF_INET;
  	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  	servaddr.sin_port = htons(port);
	if (bind(m_serverSocketHandle, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    	close();
    	throw network::BindException(strerror(errno));
    }
	if (listen(m_serverSocketHandle, 1) < 0) {
		close();
		throw network::BindException(strerror(errno));
	}  
}

// accept a new SOCK_Stream connection
int
SOCK_Acceptor::accept(SOCK_Stream &new_stream)
throw (network::ConnectException&)
{
	// init the stream
	struct sockaddr_in clientSockAddr;
	int clientSocket;
	size_t addrLen = sizeof(struct sockaddr_in);
	clientSocket = ::accept(m_serverSocketHandle,
				(struct sockaddr *)&clientSockAddr,
				&addrLen);
	if (clientSocket < 0) {
	    throw network::ConnectException(strerror(errno));
	}
	new_stream.setHandle(clientSocket);
	return 0;
}

// Close the socket, returns 0 on success and -1 on failure.
int
SOCK_Acceptor::close(void)
{
	::close(m_serverSocketHandle);
	return 0;
}

} // namespace bsd
