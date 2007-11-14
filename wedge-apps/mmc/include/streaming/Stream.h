#ifndef STREAM_H_
#define STREAM_H_

#include "bsd/SOCK_Stream.h"

namespace streaming
{
//
// base class
//
class Stream
{
public:
	Stream() {}
	virtual ~Stream() throw() {}
	void setStream(bsd::SOCK_Stream& stream) { m_stream = stream; }

protected:
	bsd::SOCK_Stream m_stream;
};

} // namespace streaming

#endif /*STREAM_H_*/
