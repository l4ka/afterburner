#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#define __USE_GNU	// for O_DIRECT
#include <fcntl.h>
#include <errno.h>


#define MIN_BLOCK_SIZE	512
#define MAX_BLOCK_SIZE	(16*4096)
#define MAX_BLOCK_MASK	~(MAX_BLOCK_SIZE-1)
#define MAX_IOVEC	1024

#define TLIMIT 10
char buffer_space[2*MAX_BLOCK_SIZE];

struct iovec iovec[MAX_IOVEC];

#define PREFIX "microdisk: "


void usage( char *progname )
{
    fprintf( stderr,  "usage: %s <device name> [block_size]\n", progname );
    exit( 1 );
}

typedef unsigned int __attribute__((__mode__(__DI__))) u64_t;
typedef unsigned int u32_t;

inline u64_t x86_rdtsc(void)
{
    u64_t __return;

    __asm__ __volatile__ (
	"rdtsc"
	: "=A"(__return));

    return __return;
}

inline u64_t x86_rdpmc(const int ctrsel)
{
    u32_t eax, edx;

    __asm__ __volatile__ (
            "rdpmc"
            : "=a"(eax), "=d"(edx)
            : "c"(ctrsel));

    return (((u64_t)edx) << 32) | (u64_t)eax;
}


#define MHZ	(3000ULL)
#define TSC_TO_MSEC(cycles)	(((cycles)) / (MHZ * 1000ULL))
#define TSC_TO_SEC(cycles)	(((cycles)) / (MHZ * 1000ULL * 1000ULL))

char *default_debug_prefix = "microdisk";

int main( int argc, char *argv[] )
{
    int dev_fd;
    char *device_name;
    char *buffer = (char *)((unsigned long)(buffer_space + MAX_BLOCK_SIZE) & MAX_BLOCK_MASK);
    unsigned long device_size;
    unsigned block_size, block_count, block;
    unsigned min_block_size, max_block_size, user_block_size = 0;
    char *debug_name, *debug_prefix;
    FILE *debug_fd = stdout;	
    int err, i, throttle = 0, tlimit = 0;
    u64_t pre_tsc = 0, post_tsc, delta_tsc, dbg_tsc = 0, throttle_tsc = 0;
    u64_t pre_uc = 0, dbg_uc = 0, delta_uc;
    long dbg_block = 0, delta_block = 0, kbs = 0, util = 0;
    
    min_block_size = MIN_BLOCK_SIZE;
    max_block_size = MAX_BLOCK_SIZE;
    debug_prefix = default_debug_prefix;
    
    if (argc == 1)
    {
	device_name = "/dev/vmblock/0";
    }
    if (argc == 2)
    {
	device_name = argv[1];
    }
    else if (argc == 3)
    {
	device_name = argv[1];
	min_block_size = max_block_size = strtoul(argv[2], NULL, 0);
    }
    else if (argc == 4)
    {
	device_name = argv[1];
	debug_name = argv[2];
	debug_prefix = argv[3];
	// Open the debug console.
	debug_fd = fopen( debug_name, "a+" );
	if( debug_fd == NULL ) {
	    fprintf( stderr, "Unable to open '%s': %s.\n",
		     device_name, strerror(errno) );
	    exit( 1 );
	}
    }
    else if (argc == 6)
    {
	device_name = argv [1];
	min_block_size = strtoul(argv[2], NULL, 0);
	max_block_size = strtoul(argv[3], NULL, 0);
	debug_name = argv[4];
	debug_prefix = argv[5];
	debug_fd = fopen( debug_name, "a+" );
	if( debug_fd == NULL ) {
	    fprintf( stderr, "Unable to open '%s': %s.\n",
		     device_name, strerror(errno) );
	    exit( 1 );
	}
    }
    else if (argc == 7)
    {
	device_name = argv[1];
	min_block_size = strtoul(argv[2], NULL, 0);
	max_block_size = strtoul(argv[3], NULL, 0);
	throttle = strtoul(argv[4], NULL, 0);
	debug_name = argv[5];
	debug_prefix = argv[6];
	debug_fd = fopen( debug_name, "a+" );
	if( debug_fd == NULL ) {
	    fprintf( stderr, "Unable to open '%s': %s.\n",
		     device_name, strerror(errno) );
	    exit( 1 );
	}
    }


    fprintf(debug_fd, "%s Running microdisk benchmark on %s blocksizes %d to %d\n",
	    debug_prefix, device_name, min_block_size, max_block_size);


    // Open the device with *raw* access.
    dev_fd = open( device_name, O_RDWR | O_DIRECT, 0 );
    if( dev_fd == -1 ) {
	fprintf( stderr, "%s Unable to open '%s': %s.\n", debug_prefix,
		 device_name, strerror(errno) );
	exit( 1 );
    }


    // Choose the number of blocks we want.
    device_size = 2048*1024*1024UL;
    block_count =  device_size / (MAX_BLOCK_SIZE * MAX_IOVEC);
    block_count *= 4;
    
    // Init the iovec array.
    for( i = 0; i < MAX_IOVEC; i++ )
	iovec[i].iov_base = buffer;

    // Print the headers.
    fflush(debug_fd);
	
    // Perform the read experiment.
    for( block_size = min_block_size; block_size <= max_block_size; block_size *= 2 )
    {
	if( lseek(dev_fd, 0, SEEK_SET) != 0 ) {
	    fprintf( debug_fd, "%s Seek error on '%s': %s.\n", 
		     debug_prefix, device_name, strerror(errno) );
	    exit( 1 );
	}

	for( i = 0; i < MAX_IOVEC; i++ )
	    iovec[i].iov_len = block_size;

	block_count = device_size / (block_size * MAX_IOVEC);
        block_count *= 4;
        
	    
	fprintf(debug_fd, "read %u\n", block_size);
	fflush(debug_fd);
		

	for( block = 0; block < block_count; block++ )
	{
	    pre_tsc = x86_rdtsc();
	    pre_uc = x86_rdpmc(0);
	    
	    err = readv(dev_fd, iovec, MAX_IOVEC);
	    if( err < 0 ) {
		fprintf( debug_fd, "%s Read error on '%s': %s.\n", 
			 debug_prefix, device_name, strerror(errno) );
		exit( 1 );
	    }
	    post_tsc = x86_rdtsc();
	    
	    // Wait throttle percent, e.g., 50 -> 1/2 delta_tsc
	    if (throttle)
	    {
		delta_tsc = post_tsc - pre_tsc;
		throttle_tsc = delta_tsc * 100 / throttle;
		while (post_tsc < (pre_tsc + delta_tsc + throttle_tsc))
                    post_tsc = x86_rdtsc();
	    }
	    
	    // Print KBS
	    delta_tsc = pre_tsc - dbg_tsc;
	    
	    if (TSC_TO_MSEC(delta_tsc) > 1000)
	    {
		delta_block = block - dbg_block;
		delta_uc   = pre_uc - dbg_uc;

		kbs = delta_block * (block_size * MAX_IOVEC);
		kbs /= TSC_TO_MSEC(delta_tsc);
		
		util = 1000 * delta_uc / delta_tsc;
		
		dbg_tsc = pre_tsc;
		dbg_block = block;
		dbg_uc = pre_uc;

		//if (tlimit)
                //  fprintf(debug_fd, "%s read, %05lu, %d MB/s %05d CPU (throttle %d)\n", 
                //    debug_prefix, block_size, kbs, util, throttle);
		
		if (tlimit++ > TLIMIT)
		{
		    fprintf(debug_fd, "\n");
		    tlimit = 0;
                    fprintf(debug_fd, "read %u done\n", block_size);
                    fflush(debug_fd);
                    break;
		}

	    }
		
	}
	fflush(debug_fd);
	    
    }

    // Perform the write experiment.
    for( block_size = min_block_size; block_size <= max_block_size; block_size *= 2 )
    {
	if( lseek(dev_fd, 0, SEEK_SET) != 0 ) {
	    fprintf( debug_fd, "%s Seek error on '%s': %s.\n", 
		     debug_prefix, device_name, strerror(errno) );
	    exit( 1 );
	}

	for( i = 0; i < MAX_IOVEC; i++ )
	    iovec[i].iov_len = block_size;

	block_count = device_size / (block_size * MAX_IOVEC);
        block_count *= 4;

	fprintf(debug_fd, "write %u\n", block_size);
	fflush(debug_fd);
	
	for( block = 0; block < block_count; block++ )
	{
	    pre_tsc = x86_rdtsc();
	    pre_uc = x86_rdpmc(0);

	    err = writev(dev_fd, iovec, MAX_IOVEC);
	    if( err < 0 ) {
		fprintf( debug_fd, "%s Write error on '%s': %s.\n", 
			 debug_prefix, device_name, strerror(errno) );
		exit( 1 );
	    }
	    post_tsc = x86_rdtsc();

	    // Wait throttle percent, e.g., 50 -> 1/2 delta_tsc
	    if (throttle)
	    {
		delta_tsc = post_tsc - pre_tsc;
		throttle_tsc = throttle * delta_tsc / 100;
		while (post_tsc < (pre_tsc + delta_tsc + throttle_tsc))
                    post_tsc = x86_rdtsc();
	    }

	    // Print KBS
	    delta_tsc = pre_tsc - dbg_tsc;
		
	    if (TSC_TO_MSEC(delta_tsc) > 1000)
	    {
		delta_block = block - dbg_block;
		delta_uc   = pre_uc - dbg_uc;

		kbs = delta_block * (block_size * MAX_IOVEC);
		kbs /= TSC_TO_MSEC(delta_tsc);
		
		util = 1000 * delta_uc / delta_tsc;
		
		dbg_tsc = pre_tsc;
		dbg_block = block;
		dbg_uc = pre_uc;

		//if (tlimit)
                //  fprintf(debug_fd, "%s write, %05lu, %d MB/s %05d CPU (throttle %d)\n", 
                //    debug_prefix, block_size, kbs, util, throttle);
		
		if (tlimit++ > TLIMIT)
		{
                    fprintf(debug_fd, "write %u done\n", block_size);
                    fflush(debug_fd); 
		    tlimit = 0;
		    break;
		}
		
	    }

	}
    }

    close( dev_fd );
}

