#ifndef STREAMINGEXCEPTION_H_
#define STREAMINGEXCEPTION_H_

#include <string>
#include <stdexcept>

namespace streaming {
	
/**
 * @class ServiceException StreamingException.h "streaming/StreamingException.h"
 * @brief streaming exception base class, indicates an error in the streaming subsystem
 */
class StreamingException : public std::runtime_error
{
	public:
		StreamingException(const std::string &st = "") : std::runtime_error(st) {}
};

} // namespace streaming

#endif /*STREAMINGEXCEPTION_H_*/
