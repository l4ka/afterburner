/*********************************************************************
 *                
 * Copyright (C) 2003-2010,  Karlsruhe University
 *                
 * File path:     mmc/src/ctrl/mmcCtrl.cpp
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
