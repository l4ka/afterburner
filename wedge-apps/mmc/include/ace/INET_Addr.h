#ifndef INET_ADDR_H_
#define INET_ADDR_H_

class ACE_INET_Addr
{
public:
	ACE_INET_Addr() {}
	~ACE_INET_Addr() {}
	int set(unsigned short port, const char *host) {
		return 0;
	}
};

#endif /*INET_ADDR_H_*/
