#ifndef NONCOPYABLE_H_
#define NONCOPYABLE_H_

namespace util
{

class NonCopyable
{
public:
	NonCopyable() {}
	virtual ~NonCopyable() throw() {}
private:
	NonCopyable(const NonCopyable&);
	NonCopyable& operator=(const NonCopyable&);
};

}; // namespace util

#endif /*NONCOPYABLE_H_*/
