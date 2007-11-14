#ifndef SOCK_CONNECTOR_H_
#define SOCK_CONNECTOR_H_

#include "ace/SOCK_Stream.h"
#include "ace/INET_Addr.h"

/**
 * @class ACE_SOCK_Connector
 * 
 * @brief defines a factory that creates new streams actively
 *
 */
class ACE_SOCK_Connector
{
public:
	ACE_SOCK_Connector(void) {}
	~ACE_SOCK_Connector(void) throw() {}

	// accept a new ACE_SOCK_Stream connection
	int connect(ACE_SOCK_Stream &new_stream, ACE_INET_Addr& addr);
};
#endif /*SOCK_CONNECTOR_H_*/
