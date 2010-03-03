/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/include/network/Acceptor.h
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
#ifndef ACCEPTOR_H_
#define ACCEPTOR_H_

#include "service/Service.h"
#include "service/ServiceManager.h"
#include "network/NetworkException.h"
#include "network/ServerSocket.h"

namespace network
{

/**
 * @class Acceptor Acceptor.h "network/Acceptor.h"
 * @brief Acceptor base class provides accept() method
 */
class Acceptor
{
public:
	Acceptor(unsigned short port, service::ServiceManager &serviceManager) : m_serviceManager(serviceManager),
	m_serverPort(port), m_running(false)
	{
	}
	virtual ~Acceptor() throw() {}
	
	void accept() throw (NetworkException&);
	void close();
	
private:
	ServerSocket m_serverSocket;
	service::ServiceManager &m_serviceManager;
	unsigned short m_serverPort;
	bool m_running;
};

}

#endif /*ACCEPTOR_H_*/
