/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/include/network/ServerSocket.h
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
#ifndef SERVERSOCKET_H_
#define SERVERSOCKET_H_

/*
 * wrapper for the SOCK_Acceptor passive connection establishment
 * provides a listening server socket, used by the Acceptor service
 * dispatcher
 */
#include "bsd/SOCK_Acceptor.h"
#include "network/Socket.h"
#include "network/NetworkException.h"

namespace network
{
	
class ServerSocket
{
public:
	ServerSocket() {}
	virtual ~ServerSocket() throw() { m_acceptor.close(); } // TODO: check exception

	std::auto_ptr<Socket> accept(void) throw (NetworkException&);
	void close() { m_acceptor.close(); }
	void init(unsigned short port) {
		m_acceptor.init(port);
	}
	
private:
	bsd::SOCK_Acceptor m_acceptor;
};

}; // namespace network

#endif /*SERVERSOCKET_H_*/
