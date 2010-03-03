/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/src/ace/SOCK_Acceptor.cpp
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
#include "ace/SOCK_Acceptor.h"

ACE_SOCK_Acceptor::ACE_SOCK_Acceptor()
{
}

ACE_SOCK_Acceptor::~ACE_SOCK_Acceptor(void) throw()
{
}

void
ACE_SOCK_Acceptor::init(unsigned short port)
throw (network::NetworkException&)
{
	WSADATA Data;
	if (WSAStartup(MAKEWORD(1, 1), &Data) != 0) {
		throw network::NetworkException("WSAStartup failed");
	}
	SOCKADDR_IN serverSockAddr;
	memset(&serverSockAddr, 0x00, sizeof(serverSockAddr));
	serverSockAddr.sin_port = htons(port);
	serverSockAddr.sin_family = AF_INET;
	serverSockAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	m_serverSocketHandle = socket(AF_INET, SOCK_STREAM, 0);
	if (m_serverSocketHandle == INVALID_SOCKET) {
		throw network::NetworkException("socket failed");
	}
	if (bind(m_serverSocketHandle, (LPSOCKADDR)&serverSockAddr, sizeof(serverSockAddr)) == SOCKET_ERROR) {
		throw network::BindException("bind failed");
	}
	if (listen(m_serverSocketHandle, 5) == SOCKET_ERROR) {
		throw network::BindException("listen failed");
	}
}

// accept a new ACE_SOCK_Stream connection
int
ACE_SOCK_Acceptor::accept(ACE_SOCK_Stream &new_stream)
{
	// init the stream
	SOCKADDR_IN clientSockAddr;
	SOCKET clientSocket;
	int addrLen = sizeof(SOCKADDR_IN);
	clientSocket = ::accept(m_serverSocketHandle,
							(LPSOCKADDR)&clientSockAddr,
							&addrLen);
	new_stream.setHandle(clientSocket);
	return 0;
}

// Close the socket, returns 0 on success and -1 on failure.
int
ACE_SOCK_Acceptor::close(void)
{
	if (WSACleanup() == SOCKET_ERROR) {
		return -1;
	}	
	return 0;
}
