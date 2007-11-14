#ifndef SERVICEEXCEPTION_H_
#define SERVICEEXCEPTION_H_

#include <string>
#include <stdexcept>

namespace service {
	
/**
 * @class ServiceException ServiceException.h "service/ServiceException.h"
 * @brief service exception base class, indicates an error in the service handlers
 */
class ServiceException : public std::runtime_error
{
	public:
		ServiceException(const std::string &st = "") : std::runtime_error(st) {}
};

} // namespace service

#endif /*SERVICEEXCEPTION_H_*/
