#ifndef ACCEPTOR_H_
#define ACCEPTOR_H_

#include "service/Service.h"
#include "service/ServiceManager.h"
#include "network/NetworkException.h"
#include "network/ServerSocket.h"

namespace network
{

/**
 * @class Acceptor Acceptor.h "network/Acceptor.h"
 * @brief Acceptor base class provides accept() method
 */
class Acceptor
{
public:
	Acceptor(unsigned short port, service::ServiceManager &serviceManager) : m_serviceManager(serviceManager),
	m_serverPort(port), m_running(false)
	{
	}
	virtual ~Acceptor() throw() {}
	
	void accept() throw (NetworkException&);
	void close();
	
private:
	ServerSocket m_serverSocket;
	service::ServiceManager &m_serviceManager;
	unsigned short m_serverPort;
	bool m_running;
};

}

#endif /*ACCEPTOR_H_*/
