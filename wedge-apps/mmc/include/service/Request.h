#ifndef REQUEST_H_
#define REQUEST_H_

#include "streaming/OutputStream.h"
#include "streaming/InputStream.h"

namespace service
{

class Request
{
public:
	static const unsigned int INVALID_REQUEST;
	static const unsigned int MIGRATION_START;
	static const unsigned int MIGRATION_RESERVE;
	static const unsigned int MIGRATION_SEND;
	
	Request(unsigned int requestType = INVALID_REQUEST)
	: m_requestType(requestType) {}
	virtual ~Request() throw() {}

	unsigned int getType(void) { return m_requestType; }
	void setType(unsigned int requestType) {
		m_requestType = requestType;
	}
	
	friend streaming::OutputStream& operator<<(streaming::OutputStream&, const Request&)
	throw (streaming::StreamingException&);
	friend streaming::InputStream& operator>>(streaming::InputStream&, Request&)
	throw (streaming::StreamingException&);

private:
	unsigned int m_requestType;
};

} // namespace service

#endif /*REQUEST_H_*/
