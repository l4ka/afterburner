#include "service/Request.h"

namespace service
{

const unsigned int Request::INVALID_REQUEST = 0;
const unsigned int Request::MIGRATION_START = 1;
const unsigned int Request::MIGRATION_RESERVE = 2;
const unsigned int Request::MIGRATION_SEND = 3;

streaming::InputStream& operator>> (streaming::InputStream& is, Request& r)
throw (streaming::StreamingException&)
{
	if (is.read((char *)&r.m_requestType, sizeof(r.m_requestType)) < 0)
		throw streaming::StreamingException("Reading Request");
	return is;
}

streaming::OutputStream& operator<< (streaming::OutputStream& os, const Request& r)
throw (streaming::StreamingException&)
{
	if (os.write((char *)&r.m_requestType, sizeof(r.m_requestType)) < 0)
		throw streaming::StreamingException("Writing Request");
	return os;
}

} // namespace service
