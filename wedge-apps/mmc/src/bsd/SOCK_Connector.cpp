#include "bsd/SOCK_Connector.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>      // memset
#include <arpa/inet.h>   // address conversion
#include <netinet/in.h>  // byte order functions

namespace bsd
{

// accept a new SOCK_Stream connection
int
SOCK_Connector::connect(SOCK_Stream &new_stream,
			const char *serverName,
			unsigned short serverPort)
{
    // init server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0x00, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverName, &serverAddr.sin_addr) < 0)
    {
	return -1;
    }
    // init the stream
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	return -1;
    }
    // connect to server
    if (::connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
	return -1;
    }
    new_stream.setHandle(sockfd);
    return 0;
}

} // namespace bsd
