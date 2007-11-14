#include "service/Request.h"
#include "streaming/OutputStream.h"

namespace streaming
{

int
OutputStream::write(const char *buffer, unsigned int len)
{
	if (len == 0) return 0;
	int bytes_read;
	if ((bytes_read = m_stream.send_n(buffer, len)) < 0)
	{
		return -1;
	}
	return bytes_read;
}

int
OutputStream::copyBuffer(char *buffer, unsigned int len)
throw (StreamingException&)
{
	// write buffer
	if ((len = write(buffer, len)) < 0)
		throw StreamingException("write failed");
	return len;
}

} // namespace streaming
