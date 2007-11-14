#ifndef CONNECTOR_H_
#define CONNECTOR_H_

#include "network/Socket.h"
#include "network/NetworkException.h"
#include "ace/INET_Addr.h"

namespace network
{

/**
 * @class Connector Connector.h "network/Connector.h"
 * @brief Connector class used to actively open a connection to
 * a remote server
 */
class Connector
{
public:
	Connector() {}
	virtual ~Connector() throw() {}
	
	std::auto_ptr<Socket> connect(unsigned short port, const char *server) 
	throw (ConnectException&);	
private:
	unsigned short m_destPort;
};

} // namespace network
#endif /*CONNECTOR_H_*/
