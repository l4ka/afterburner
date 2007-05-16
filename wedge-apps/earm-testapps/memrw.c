#define LIMIT (1 << 28) /* 256 Mb */
#define CACHELINE (1<<7) 

int main()
	{
	int *x=(int*)malloc(LIMIT);
	int y;
	volatile int *z,a;
	if (!x) 
		{
		printf("Cold not allocate memory:(\n");
		exit(1);
		}
	while(42)
		{
		y=random()%(CACHELINE/sizeof(int))+CACHELINE/sizeof(int);
		for (z=x;z<x+LIMIT/sizeof(int);z+=y)
			{
			a=*z;
			*z=a;
			}
		}
	}
