#ifndef SOCK_ACCEPTOR_H_
#define SOCK_ACCEPTOR_H_

#include "bsd/SOCK_Stream.h"
#include "network/NetworkException.h"

namespace bsd
{

/**
 * @class SOCK_Acceptor
 * 
 * @brief defines a factory that creates new streams passivly
 *
 */
class SOCK_Acceptor
{
public:
	SOCK_Acceptor();
	~SOCK_Acceptor(void) throw();

	// accept a new ACE_SOCK_Stream connection
	int accept(SOCK_Stream &new_stream) throw(network::ConnectException&);
	void init(unsigned short port) throw(network::NetworkException&);

	// Close the socket
	// Returns 0 on success and -1 on failure.
	int close (void);

private:
	int m_serverSocketHandle;
};

} // namespace bsd

#endif /*SOCK_ACCEPTOR_H_*/
