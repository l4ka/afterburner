#include "network/Acceptor.h"
#include "service/ServiceManager.h"
#include "network/NetworkException.h"

#include <iostream>

int main (int argc, char **argv)
{
	unsigned int port = 6666;
	service::ServiceManager serviceManager;
	
	std::cout << "running AcceptorTest..." << std::endl;
	
	try {
		network::Acceptor acceptor(port, serviceManager);
		acceptor.accept();
	} catch (network::NetworkException &ne) {
		std::cerr << "acceptor failed" << std::endl;
	}
	
	std::cout << "done" << std::endl;
	
	return 0;
}
