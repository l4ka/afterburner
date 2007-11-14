#ifndef SERVICEMANAGER_H_
#define SERVICEMANAGER_H_

#include "service/Service.h"
#include "network/Socket.h"

namespace service
{

/**
 * @class ServiceManager ServiceManager.h "service/ServiceManager.h"
 * @brief provides a ServiceType to Service mapping,
 * ServiceTypes are read from a Socket connection
 */
class ServiceManager
{
public:
	ServiceManager() {}
	virtual ~ServiceManager() throw() {}

    Service *getService(std::auto_ptr<network::Socket>& connection);

private:
	static const int SERVICE_TYPE_MIGRATION;
	static const int SERVICE_TYPE_POWER_MANAGEMENT;
};

}; // namespace network

#endif /*SERVICEMANAGER_H_*/
