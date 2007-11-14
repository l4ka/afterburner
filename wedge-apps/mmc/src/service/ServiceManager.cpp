#include "service/ServiceManager.h"
#include "service/MigrationService.h"

namespace service
{

const int ServiceManager::SERVICE_TYPE_MIGRATION = 1;
const int ServiceManager::SERVICE_TYPE_POWER_MANAGEMENT = 2;

Service *
ServiceManager::getService(std::auto_ptr<network::Socket>& connection)
{
	int serviceType = SERVICE_TYPE_MIGRATION;
	//InputStream in = Socket.getInputStream();
	//serviceType << in;
	switch (serviceType)
	{
		case SERVICE_TYPE_MIGRATION:
			return new MigrationService();
		default:
			return NULL;
	}
}

}; // namespace service
