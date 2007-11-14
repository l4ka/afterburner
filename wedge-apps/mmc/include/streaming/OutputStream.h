#ifndef OUTPUTSTREAM_H_
#define OUTPUTSTREAM_H_

#include "streaming/StreamingException.h"
#include "streaming/Stream.h"

namespace streaming
{

class OutputStream : public Stream
{
public:
	int write(const char *buffer, unsigned int len);
	int copyBuffer(char *buffer, unsigned int len)
	throw (StreamingException&);
};

} // namespace streaming

#endif /*OUTPUTSTREAM_H_*/
