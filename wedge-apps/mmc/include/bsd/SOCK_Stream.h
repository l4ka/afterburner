#ifndef SOCK_STREAM_H_
#define SOCK_STREAM_H_

#include <cstddef>
#include <sys/types.h>

namespace bsd
{

/**
 * @class SOCK_Stream
 * 
 * @brief stream socket communication
 * 
 */
class SOCK_Stream
{
public:
	SOCK_Stream(void) : m_handle(-1) {}
	~SOCK_Stream(void) {}
	
	// try to receive exactly len bytes from the connected
	// socket to buf
	ssize_t recv_n(void *buf,
				size_t len,
				const unsigned long *timeout = 0,
				size_t *bytes_transferred = 0);
	
	// try to send exactly len bytes from buf to
	// the connected socket
	ssize_t send_n(const void *buf,
				size_t len,
				const unsigned long *timeout = 0,
				size_t *bytes_transferred = 0);
	
	int close(void);
	void setHandle(int handle) { m_handle = handle; }
	
private:
	int m_handle;
};

} // namespace bsd

#endif /*SOCK_STREAM_H_*/
