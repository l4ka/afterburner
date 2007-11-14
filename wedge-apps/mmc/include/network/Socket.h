#ifndef SOCKET_H_
#define SOCKET_H_

#include "bsd/SOCK_Stream.h"
#include "util/NonCopyable.h"
#include "streaming/InputStream.h"
#include "streaming/OutputStream.h"

namespace network
{

/**
 * @class Socket Socket.h "network/Socket.h"
 * @brief wrapper for SOCK_Stream, represents a client network connection endpoint,
 *        a Socket is implicitly opened when a new Socket object is instantiated,
 *        a Socket can be closed explicitly or by using the destructor
 */ 
class Socket : public util::NonCopyable
{
public:
	Socket() {}
	virtual ~Socket() throw() { m_clientConnection.close(); } // TODO: check exception

	void close(void) { m_clientConnection.close(); }
	streaming::InputStream& getInputStream() {
		m_inputStream.setStream(m_clientConnection);
		return m_inputStream;
	}
	streaming::OutputStream& getOutputStream() {
		m_outputStream.setStream(m_clientConnection);
		return m_outputStream;
	}
	bsd::SOCK_Stream& getConnectionHandle() {
		return m_clientConnection;
	}

private:
	streaming::InputStream m_inputStream;
	streaming::OutputStream m_outputStream;
	bsd::SOCK_Stream m_clientConnection;
};

};

#endif /*SOCKET_H_*/
