#include "network/Acceptor.h"
#include "service/ServiceManager.h"
#include "network/NetworkException.h"

#include <iostream>

static void usage()
{
	std::cerr << "usage: mmcd port" << std::endl;
	exit(EXIT_FAILURE);
}

int main (int argc, char **argv)
{
	if (argc != 2) usage();
	unsigned short port = atoi(argv[1]);
	service::ServiceManager serviceManager;
	
	std::cout << "starting mmcd acceptor on port " << port << std::endl;
	
	try {
		network::Acceptor acceptor(port, serviceManager);
		acceptor.accept();
	} catch (network::NetworkException &ne) {
		std::cerr << "acceptor failed" << std::endl;
	}
	
	std::cout << "done" << std::endl;
	
	return 0;
}
