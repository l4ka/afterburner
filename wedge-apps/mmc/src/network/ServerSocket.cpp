#include "network/ServerSocket.h"

/*
 * wrapper for a listening server socket communication endpoint
 */

namespace network
{
	
std::auto_ptr<Socket>
ServerSocket::accept(void)
throw (NetworkException&)
{
	std::auto_ptr<Socket> clientConnectionPtr(new Socket());
	bsd::SOCK_Stream& clientStream = clientConnectionPtr->getConnectionHandle();
	if (m_acceptor.accept(clientStream) == -1)
		throw NetworkException("accept failed");
	return clientConnectionPtr;
}

};
