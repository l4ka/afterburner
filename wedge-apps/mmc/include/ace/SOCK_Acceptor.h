#ifndef SOCK_ACCEPTOR_H_
#define SOCK_ACCEPTOR_H_

#include "ace/SOCK_Stream.h"
#include "network/NetworkException.h"

#include <winsock.h>


/**
 * @class ACE_SOCK_Acceptor
 * 
 * @brief defines a factory that creates new streams passivly
 *
 */
class ACE_SOCK_Acceptor
{
public:
	ACE_SOCK_Acceptor();
	~ACE_SOCK_Acceptor(void) throw();

	// accept a new ACE_SOCK_Stream connection
	int accept(ACE_SOCK_Stream &new_stream);
	void init(unsigned short port) throw(network::NetworkException&);

	// Close the socket
	// Returns 0 on success and -1 on failure.
	int close (void);

private:
	SOCKET m_serverSocketHandle;
};




#endif /*SOCK_ACCEPTOR_H_*/
