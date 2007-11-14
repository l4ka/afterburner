#include "service/MigrationService.h"
#include "service/Request.h"
#include "streaming/InputStream.h"
#include "streaming/OutputStream.h"
#include "streaming/StreamingException.h"
#include "l4app/Resourcemon.h"
#include "network/Connector.h"

#include <iostream>

namespace service
{

MigrationService::MigrationService()
{
}

MigrationService::~MigrationService()
{
}

/*
 * start a migration process on the local host,
 * initiated by the mmcCtrl application
 */
void
MigrationService::handleMigrationStartRequest(std::auto_ptr<network::Socket>& connectionPtr)
throw (ServiceException&)
{
	// read VMInfo from local RM
	l4app::Resourcemon resourcemon;
	l4app::VMInfo vmInfo;
	unsigned int spaceId = 0;
	resourcemon.getVMInfo(spaceId, vmInfo);

	try {
		// connect to destination
		network::Connector connector;
		unsigned short port = 6666;
		const char *host = "localhost";
		std::auto_ptr<network::Socket> socketPtr = connector.connect(port, host);
		
		// send reserve request to destination
		streaming::OutputStream outputStream = socketPtr->getOutputStream();
		Request reserveRequest(Request::MIGRATION_RESERVE);
		outputStream << reserveRequest;
		outputStream.copyBuffer(reinterpret_cast<char *>(&vmInfo), sizeof(vmInfo));
		
		// check the reserve reply
		streaming::InputStream inputStream = socketPtr->getInputStream();
		Request reply;
		inputStream >> reply;
		
		// map VM space and send to destination
		char *vm_space = NULL;
		resourcemon.getVMSpace(spaceId, vm_space);
		Request sendRequest(Request::MIGRATION_SEND);
		outputStream << sendRequest;
		outputStream.copyBuffer(vm_space, vmInfo.get_space_size());

		// check send reply
		inputStream >> reply;
		
		// delete local VM
		resourcemon.deleteVM(spaceId);
	} catch (network::ConnectException& ce) {
		std::cerr << "ConnectException: " << ce.what() << std::endl;
		throw ServiceException("Migration Start Request Failed");
	} catch (streaming::StreamingException& se) {
		std::cerr << "StreamingException: " << se.what() << std::endl;
		throw ServiceException("Migration Start Request Failed");
	}
}

/**
 * @brief reserve a new VM from the local RM using VMInfo sent
 * 		  by migration source host,
 * 		  read VM space from source host and map into local RM
 */
void
MigrationService::handleMigrationMigrateRequest(std::auto_ptr<network::Socket>& connectionPtr)
throw (ServiceException&)
{
	streaming::InputStream inputStream = connectionPtr->getInputStream();
	// read VMInfo
	l4app::VMInfo vmInfo;
	if (inputStream.read(reinterpret_cast<char *>(&vmInfo), sizeof(vmInfo)) < 0)
		throw ServiceException("reading VMInfo");
	// reserve VM
	l4app::Resourcemon resourcemon;
	L4_Word_t spaceId;
	if (resourcemon.reserveVM(vmInfo, &spaceId) < 0)
		throw ServiceException("reserving VM");	
	// send reply
	streaming::OutputStream outputStream = connectionPtr->getOutputStream();
	Request reply;
	outputStream << reply;
	// read VM space
	int len = (1<<27);
	char *vmSpace = new char(len); // TODO: determine correct len
	if (inputStream.read(vmSpace, len) < 0)
		throw ServiceException("reading VM space");
	// map to RM
	if (resourcemon.mapVMSpace(spaceId, vmSpace) < 0)
		throw ServiceException("mapping VM space");
	outputStream << reply;
}

/**
 * @brief called from Acceptor after the Socket connection has been established,
 *        assumes that the Socket connection is valid during the request handling,
 *        throws NetworkException if Socket becomes invalid
 */
void
MigrationService::handleRequest(std::auto_ptr<network::Socket>& connectionPtr)
throw (ServiceException&)
{
	streaming::InputStream inputStream = connectionPtr->getInputStream();
	Request request;
	inputStream >> request;
	switch (request.getType())
	{
		#define MIGRATION_START 1
		#define MIGRATION_MIGRATE 2
		
		//case Request::MIGRATION_START:
		case MIGRATION_START:
			handleMigrationStartRequest(connectionPtr);
			break;
		case MIGRATION_MIGRATE:
			handleMigrationMigrateRequest(connectionPtr);
			break;
		default:
			throw ServiceException("invaid request type" + request.getType());
	}
}

}; // namespace service
