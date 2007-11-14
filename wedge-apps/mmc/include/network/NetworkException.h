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
