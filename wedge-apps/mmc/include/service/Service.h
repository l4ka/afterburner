#ifndef SERVICE_H_
#define SERVICE_H_

#include "network/Socket.h"

namespace service
{
	
class Service
{
public:
	Service() {}
	virtual ~Service() {}

	virtual void handleRequest(std::auto_ptr<network::Socket>& connection) = 0;
};

}; // namespace service

#endif /*SERVICE_H_*/
