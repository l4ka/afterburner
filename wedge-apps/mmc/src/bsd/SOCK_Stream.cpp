#include "bsd/SOCK_Stream.h"

#include <sys/socket.h>
#include <unistd.h>

namespace bsd
{

// try to receive exactly len bytes from the connected
// socket to buf
ssize_t
SOCK_Stream::recv_n(void *buf, size_t len,
		    const unsigned long *timeout,
		    size_t *bytes_transferred)
{
    if (len == 0) {
	return 0;
    }
    size_t total_bytes_received = 0;
    ssize_t bytes_received;
    do {
	bytes_received = ::recv(m_handle,
				(static_cast<char *>(buf)) + total_bytes_received,
				len - total_bytes_received, 0);
	if (bytes_received <= 0) {
	    return -1;
	}
	total_bytes_received += bytes_received;
    } while (total_bytes_received != len);
    return len;
}

// try to send exactly len bytes from buf to
// the connected socket
ssize_t
SOCK_Stream::send_n(const void *buf,
		    size_t len,
		    const unsigned long *timeout,
		    size_t *bytes_transferred)
{
    if (len == 0) { return 0; }
    size_t total_bytes_sent = 0;
    ssize_t bytes_sent;
    do {
	bytes_sent = ::send(m_handle,
			    (static_cast<const char *>(buf)) + total_bytes_sent,
			    len - total_bytes_sent, 0);
	if (bytes_sent <= 0) {
	    return -1;
	}
	total_bytes_sent += bytes_sent;
    } while (total_bytes_sent != len);
    return len;
}

// close the stream
int
SOCK_Stream::close(void)
{
    ::close(m_handle);
    return 0;
}

} // namespace bsd
