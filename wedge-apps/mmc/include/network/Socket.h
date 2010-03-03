/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/include/network/Socket.h
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
#ifndef SOCKET_H_
#define SOCKET_H_

#include "bsd/SOCK_Stream.h"
#include "util/NonCopyable.h"
#include "streaming/InputStream.h"
#include "streaming/OutputStream.h"

namespace network
{

/**
 * @class Socket Socket.h "network/Socket.h"
 * @brief wrapper for SOCK_Stream, represents a client network connection endpoint,
 *        a Socket is implicitly opened when a new Socket object is instantiated,
 *        a Socket can be closed explicitly or by using the destructor
 */ 
class Socket : public util::NonCopyable
{
public:
	Socket() {}
	virtual ~Socket() throw() { m_clientConnection.close(); } // TODO: check exception

	void close(void) { m_clientConnection.close(); }
	streaming::InputStream& getInputStream() {
		m_inputStream.setStream(m_clientConnection);
		return m_inputStream;
	}
	streaming::OutputStream& getOutputStream() {
		m_outputStream.setStream(m_clientConnection);
		return m_outputStream;
	}
	bsd::SOCK_Stream& getConnectionHandle() {
		return m_clientConnection;
	}

private:
	streaming::InputStream m_inputStream;
	streaming::OutputStream m_outputStream;
	bsd::SOCK_Stream m_clientConnection;
};

};

#endif /*SOCKET_H_*/
