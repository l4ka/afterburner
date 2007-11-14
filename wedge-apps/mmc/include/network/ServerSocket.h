#ifndef SERVERSOCKET_H_
#define SERVERSOCKET_H_

/*
 * wrapper for the SOCK_Acceptor passive connection establishment
 * provides a listening server socket, used by the Acceptor service
 * dispatcher
 */
#include "bsd/SOCK_Acceptor.h"
#include "network/Socket.h"
#include "network/NetworkException.h"

namespace network
{
	
class ServerSocket
{
public:
	ServerSocket() {}
	virtual ~ServerSocket() throw() { m_acceptor.close(); } // TODO: check exception

	std::auto_ptr<Socket> accept(void) throw (NetworkException&);
	void close() { m_acceptor.close(); }
	void init(unsigned short port) {
		m_acceptor.init(port);
	}
	
private:
	bsd::SOCK_Acceptor m_acceptor;
};

}; // namespace network

#endif /*SERVERSOCKET_H_*/
