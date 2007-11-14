#ifndef SOCK_CONNECTOR_H_
#define SOCK_CONNECTOR_H_

#include "bsd/SOCK_Stream.h"

namespace bsd
{

/**
 * @class SOCK_Connector
 * 
 * @brief defines a factory that creates new streams actively
 *
 */
class SOCK_Connector
{
public:
	SOCK_Connector(void) {}
	~SOCK_Connector(void) throw() {}

	// accept a new SOCK_Stream connection
	int connect(SOCK_Stream &new_stream, const char *serverName,
				unsigned short port);
};

} // namespace bsd

#endif /*SOCK_CONNECTOR_H_*/
