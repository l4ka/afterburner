#ifndef MIGRATIONSERVICE_H_
#define MIGRATIONSERVICE_H_

#include "service/Service.h"
#include "service/ServiceException.h"
#include "service/Request.h"

namespace service
{

class MigrationService : public Service
{
	typedef std::auto_ptr<network::Socket> SocketPtr;
public:
	MigrationService();
	virtual ~MigrationService();

	// override handleRequest method from Service base class
	void handleRequest(SocketPtr& connection) throw (ServiceException&);

private:
	void handleMigrationStartRequest(SocketPtr& connectionPtr) throw (ServiceException&);
	void handleMigrationMigrateRequest(SocketPtr& connectionPtr) throw (ServiceException&);
};

} // namespace service

#endif /*MIGRATIONSERVICE_H_*/
