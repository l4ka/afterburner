/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/src/service/Request.cpp
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
#include "service/Request.h"

namespace service
{

const unsigned int Request::INVALID_REQUEST = 0;
const unsigned int Request::MIGRATION_START = 1;
const unsigned int Request::MIGRATION_RESERVE = 2;
const unsigned int Request::MIGRATION_SEND = 3;

streaming::InputStream& operator>> (streaming::InputStream& is, Request& r)
throw (streaming::StreamingException&)
{
	if (is.read((char *)&r.m_requestType, sizeof(r.m_requestType)) < 0)
		throw streaming::StreamingException("Reading Request");
	return is;
}

streaming::OutputStream& operator<< (streaming::OutputStream& os, const Request& r)
throw (streaming::StreamingException&)
{
	if (os.write((char *)&r.m_requestType, sizeof(r.m_requestType)) < 0)
		throw streaming::StreamingException("Writing Request");
	return os;
}

} // namespace service
