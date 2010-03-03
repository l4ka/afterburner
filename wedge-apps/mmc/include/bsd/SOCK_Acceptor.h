/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/include/bsd/SOCK_Acceptor.h
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
#ifndef SOCK_ACCEPTOR_H_
#define SOCK_ACCEPTOR_H_

#include "bsd/SOCK_Stream.h"
#include "network/NetworkException.h"

namespace bsd
{

/**
 * @class SOCK_Acceptor
 * 
 * @brief defines a factory that creates new streams passivly
 *
 */
class SOCK_Acceptor
{
public:
	SOCK_Acceptor();
	~SOCK_Acceptor(void) throw();

	// accept a new ACE_SOCK_Stream connection
	int accept(SOCK_Stream &new_stream) throw(network::ConnectException&);
	void init(unsigned short port) throw(network::NetworkException&);

	// Close the socket
	// Returns 0 on success and -1 on failure.
	int close (void);

private:
	int m_serverSocketHandle;
};

} // namespace bsd

#endif /*SOCK_ACCEPTOR_H_*/
