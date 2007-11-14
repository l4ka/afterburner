#ifndef INPUTSTREAM_H_
#define INPUTSTREAM_H_

#include "streaming/Stream.h"
#include "ace/SOCK_Stream.h"

namespace streaming
{

class InputStream : public Stream
{
public:	
	int read(char *buffer, int len);
};

} // namespace streaming

#endif /*INPUTSTREAM_H_*/
