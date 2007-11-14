#include "network/Acceptor.h"
#include "network/NetworkException.h"
#include "service/Service.h"
#include "service/ServiceException.h"

#include <iostream>

namespace network
{

void
Acceptor::accept(void) throw (NetworkException&)
{
	m_serverSocket.init(m_serverPort);
	m_running = true;
	while (m_running)
	{
		std::auto_ptr<Socket> connection = m_serverSocket.accept();
		service::Service *service = m_serviceManager.getService(connection);
		try {
			service->handleRequest(connection);
		} catch (service::ServiceException& se) {
			std::cerr << "ServiceException: " << se.what() << std::endl;
		}
		m_running = false; // XXX single thread test
	} // while (running)
}

void
Acceptor::close(void)
{
	m_running = false;
}

}; // namespace network
