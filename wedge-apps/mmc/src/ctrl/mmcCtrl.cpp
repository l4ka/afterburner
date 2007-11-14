#include <iostream>
#include <cstdlib>

#include "network/Connector.h"
#include "service/Request.h"

static char *command = NULL;
static char *host = NULL;
static unsigned short port = 0;

static void usage()
{
	std::cerr << "usage: mmcCtrl host port command" << std::endl;
	exit(EXIT_FAILURE);
}

static void parseCommandLine(int argc, char **argv)
{
	if (argc != 4) usage();
	host = argv[1];
	port = atoi(argv[2]);
	command = argv[3];
}

static void executeMigrate()
{
	// connect to MMC server
	network::Connector connector;
	try {
		std::auto_ptr<Socket> socketPtr = connector.connect(port, host);
	} catch (network::NetworkException& ne) {
		std::cerr << "caught NetworkException: " << ne.what() << std::endl;
	}
	// send migration start request
	Request migrationStartRequest(Request::MIGRATION_START);
	streaming::OutputStream os = socketPtr->getOutputStream();
	os << migrationStartRequest;
}

static void executeCommand(const char *command)
{
	if (strlen(command) == strlen("migrate") &&
		!strncmp(command, "migrate", strlen("migrate"))) {
		executeMigrate();
	}
}

int main(int argc, char **argv)
{
	parseCommandLine(argc, argv);
	executeCommand(command);
	return EXIT_SUCCESS;
}
