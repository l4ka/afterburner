/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/include/service/Request.h
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
#ifndef REQUEST_H_
#define REQUEST_H_

#include "streaming/OutputStream.h"
#include "streaming/InputStream.h"

namespace service
{

class Request
{
public:
	static const unsigned int INVALID_REQUEST;
	static const unsigned int MIGRATION_START;
	static const unsigned int MIGRATION_RESERVE;
	static const unsigned int MIGRATION_SEND;
	
	Request(unsigned int requestType = INVALID_REQUEST)
	: m_requestType(requestType) {}
	virtual ~Request() throw() {}

	unsigned int getType(void) { return m_requestType; }
	void setType(unsigned int requestType) {
		m_requestType = requestType;
	}
	
	friend streaming::OutputStream& operator<<(streaming::OutputStream&, const Request&)
	throw (streaming::StreamingException&);
	friend streaming::InputStream& operator>>(streaming::InputStream&, Request&)
	throw (streaming::StreamingException&);

private:
	unsigned int m_requestType;
};

} // namespace service

#endif /*REQUEST_H_*/
