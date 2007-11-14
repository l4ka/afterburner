#include "streaming/InputStream.h"
#include "service/Request.h"

namespace streaming
{

int
InputStream::read(char *buffer, int len)
{
	if (len == 0) return 0;
	int bytes_read;
	if ((bytes_read = m_stream.recv_n(buffer, len)) < 0)
	{
		return -1;
	}
	return bytes_read;
}

} // namespace streaming
