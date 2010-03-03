/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/include/network/NetworkException.h
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
#ifndef NETWORKEXCEPTION_H_
#define NETWORKEXCEPTION_H_

#include <string>
#include <stdexcept>

namespace network {
	
/**
 * @class NetworkException NetworkException.h "network/Exception.h"
 * @brief network exception base class, indicates an error in the underlying network protocols
 */
class NetworkException : public std::runtime_error
{
	public:
		NetworkException(const std::string &st = "") : std::runtime_error(st) {}
};

/**
 * @class BindException NetworkException.h "network/NetworkException.h"
 * @brief bind exception is raised when an error occures while a socket is bind to a local
 *        network address and port
 */ 
class BindException : public NetworkException
{
	public:
		BindException(const std::string &st = "") : NetworkException(st) {}
};

/**
 * @class ConnectException NetworkException.h "network/NetworkException.h"
 * @brief connect exception is raised when an error occurs during the connection attempt to a remote
 *        host
 */
class ConnectException : public NetworkException
{
	public:
		ConnectException(const std::string &st = "") : NetworkException(st) {}
};

}; // namespace network

#endif /*NETWORKEXCEPTION_H_*/
