#include "ace/SOCK_Acceptor.h"

ACE_SOCK_Acceptor::ACE_SOCK_Acceptor()
{
}

ACE_SOCK_Acceptor::~ACE_SOCK_Acceptor(void) throw()
{
}

void
ACE_SOCK_Acceptor::init(unsigned short port)
throw (network::NetworkException&)
{
	WSADATA Data;
	if (WSAStartup(MAKEWORD(1, 1), &Data) != 0) {
		throw network::NetworkException("WSAStartup failed");
	}
	SOCKADDR_IN serverSockAddr;
	memset(&serverSockAddr, 0x00, sizeof(serverSockAddr));
	serverSockAddr.sin_port = htons(port);
	serverSockAddr.sin_family = AF_INET;
	serverSockAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	m_serverSocketHandle = socket(AF_INET, SOCK_STREAM, 0);
	if (m_serverSocketHandle == INVALID_SOCKET) {
		throw network::NetworkException("socket failed");
	}
	if (bind(m_serverSocketHandle, (LPSOCKADDR)&serverSockAddr, sizeof(serverSockAddr)) == SOCKET_ERROR) {
		throw network::BindException("bind failed");
	}
	if (listen(m_serverSocketHandle, 5) == SOCKET_ERROR) {
		throw network::BindException("listen failed");
	}
}

// accept a new ACE_SOCK_Stream connection
int
ACE_SOCK_Acceptor::accept(ACE_SOCK_Stream &new_stream)
{
	// init the stream
	SOCKADDR_IN clientSockAddr;
	SOCKET clientSocket;
	int addrLen = sizeof(SOCKADDR_IN);
	clientSocket = ::accept(m_serverSocketHandle,
							(LPSOCKADDR)&clientSockAddr,
							&addrLen);
	new_stream.setHandle(clientSocket);
	return 0;
}

// Close the socket, returns 0 on success and -1 on failure.
int
ACE_SOCK_Acceptor::close(void)
{
	if (WSACleanup() == SOCKET_ERROR) {
		return -1;
	}	
	return 0;
}
