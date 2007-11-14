#include "bsd/SOCK_Acceptor.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

namespace bsd
{

SOCK_Acceptor::SOCK_Acceptor()
{
}

SOCK_Acceptor::~SOCK_Acceptor(void) throw()
{
}

void
SOCK_Acceptor::init(unsigned short port)
throw (network::NetworkException&)
{
	// create server socket, bind and listen
  	if ((m_serverSocketHandle = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      throw network::NetworkException(strerror(errno));
    }
	int flag = 1;
	if (setsockopt(m_serverSocketHandle, SOL_SOCKET, SO_REUSEADDR, (void *)&flag,
                 	sizeof(int)) < 0) {
      	close();
      	throw network::NetworkException(strerror(errno));
  	}
	struct sockaddr_in servaddr;
  	memset(&servaddr, 0x00, sizeof(servaddr));
  	servaddr.sin_family = AF_INET;
  	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  	servaddr.sin_port = htons(port);
	if (bind(m_serverSocketHandle, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    	close();
    	throw network::BindException(strerror(errno));
    }
	if (listen(m_serverSocketHandle, 1) < 0) {
		close();
		throw network::BindException(strerror(errno));
	}  
}

// accept a new SOCK_Stream connection
int
SOCK_Acceptor::accept(SOCK_Stream &new_stream)
throw (network::ConnectException&)
{
	// init the stream
	struct sockaddr_in clientSockAddr;
	int clientSocket;
	size_t addrLen = sizeof(struct sockaddr_in);
	clientSocket = ::accept(m_serverSocketHandle,
				(struct sockaddr *)&clientSockAddr,
				&addrLen);
	if (clientSocket < 0) {
	    throw network::ConnectException(strerror(errno));
	}
	new_stream.setHandle(clientSocket);
	return 0;
}

// Close the socket, returns 0 on success and -1 on failure.
int
SOCK_Acceptor::close(void)
{
	::close(m_serverSocketHandle);
	return 0;
}

} // namespace bsd
