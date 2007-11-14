#include "network/Connector.h"
#include "network/NetworkException.h"
#include "bsd/SOCK_Connector.h"

namespace network
{

std::auto_ptr<Socket>
Connector::connect(unsigned short port, const char *host)
throw (ConnectException&)
{
 	// connect to server
 	bsd::SOCK_Connector connector;
 	std::auto_ptr<Socket> clientConnectionPtr(new Socket);
	bsd::SOCK_Stream peer = clientConnectionPtr->getConnectionHandle();
	if (connector.connect(peer, host, port) < 0)
		throw ConnectException("could not to connect to server");
	return clientConnectionPtr;
}
      
} // namespace network
