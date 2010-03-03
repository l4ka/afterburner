/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/include/ace/SOCK_Connector.h
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
#ifndef SOCK_CONNECTOR_H_
#define SOCK_CONNECTOR_H_

#include "ace/SOCK_Stream.h"
#include "ace/INET_Addr.h"

/**
 * @class ACE_SOCK_Connector
 * 
 * @brief defines a factory that creates new streams actively
 *
 */
class ACE_SOCK_Connector
{
public:
	ACE_SOCK_Connector(void) {}
	~ACE_SOCK_Connector(void) throw() {}

	// accept a new ACE_SOCK_Stream connection
	int connect(ACE_SOCK_Stream &new_stream, ACE_INET_Addr& addr);
};
#endif /*SOCK_CONNECTOR_H_*/
