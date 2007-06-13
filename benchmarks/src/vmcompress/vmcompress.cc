#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <zlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>

bool verbose;
size_t pagesize = 4096;
char * wsext = NULL;


class page_t {
public:
    unsigned long hash;
    size_t compressed;
    long long shared, active;

    bool is_zero_page() {
	return hash == 0;
    }

    bool is_balloon_page() {
	return hash == (unsigned long)(0x1f1f1f1fULL * 1024ULL);
    }
};

class vmimg_t {
public:
    vmimg_t(char * filename) 
	{ 
	    this->filename = filename; 
	    fd = 0;
	    start = NULL;
	    zeropages = comppages = balloonpages = activepages = 0;
	    compsize = 0;
	}

    bool init()
	{
	    if (verbose) printf("initializing vm image file %s\n", filename);
	    fd = open(filename, 0);
	    if (fd < 0) {
		fprintf(stderr, "opening %s failed\n", filename);
		return false;
	    }

	    struct stat stat;
	    if (fstat (fd, &stat) < 0) {
		fprintf(stderr, "failed to fstat %s\n", filename);
		close(fd);
		return false;
	    }

	    size = stat.st_size;
	    if (verbose) printf("file size of %s: %d\n", filename, size);

	    start = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
	    if (start == MAP_FAILED) {
		fprintf(stderr, "failed to mmap file\n");
		close (fd);
		return false;
	    }
	    if (verbose) printf("mmapped %d bytes to to %p\n", size, start);

	    pages = (page_t*)calloc((size / pagesize) + 1, sizeof(page_t));
	    if (!pages) {
		fprintf(stderr, "failed to allocated page array\n");
		return false;
	    }

	    for (unsigned pgidx = 0; pgidx < get_pages(); pgidx++) {
		pages[pgidx].compressed = Compress(pgidx * pagesize, pagesize);
		pages[pgidx].hash = Hash(pgidx * pagesize, pagesize);
	    }

	    if (wsext) {
		// XXXX
		char buf[512];
		FILE *ws;
		strcpy(buf, filename);
		strcat(buf, wsext);
		if (verbose) printf("trying to open working set file %s...\n", buf);
		if ((ws = fopen(buf, "r")))
		{
		    unsigned long val;
		    unsigned ret;
		    while(((ret = fscanf(ws, "%d%[,\n]", &val, &buf)) != EOF) && (ret != 0))
		    {
			if (verbose) printf("%x, ", val);
#warning hack: always mark zero pages inactive
			if (val < size && !pages[val/pagesize].is_zero_page())
			    pages[val/pagesize].active = 1;
		    }
		    if (verbose) printf("\n");
		}
	    }
	    return true;
	}

    size_t Compress(off_t offset, size_t len) 
	{
	    assert (start);
	    assert (offset + len <= size);
	    uLongf newlen;
	    Bytef buf[len + 4096];
	    if (compress(buf, &newlen, (Bytef*)((off_t)start + offset), (ulong)len) == Z_OK)
		return newlen;
	    return 0;
	}

    unsigned long Hash(off_t offset, size_t len)
	{
	    assert(start);
	    assert(offset + len <= size);
	    unsigned long * ptr = (unsigned long*)((off_t)start + offset);
	    unsigned long hash = 0;
	    len /= sizeof(unsigned long);
	    while (len--)
		hash += *ptr++;
	    return hash;
	}

    off_t get_size() {
	return size;
    }

    unsigned long get_pages() {
	return size / pagesize;
    }

    int fd;
    char * filename;
    void * start;
    off_t size;
    page_t * pages;

    unsigned zeropages;
    unsigned balloonpages;
    unsigned activepages;
    unsigned compsize;
    unsigned comppages;
};


unsigned numimg;
vmimg_t ** vmimages;

void print_help() {
    printf("usage:\n"
	   "-s <size>	page size (default 4K)\n"
	   "-w ext	working set file extension\n"
	   "-v		verbose\n");
}

int main(int argc, char ** argv)
{
    char c;

    while ((c = getopt(argc, argv, "hv?s:w:")) != -1 )
    {
        switch (c)
        {
        case 'v':
            verbose = true; break;
	case 's':
	    if (optarg) pagesize = atoi(optarg); break;
	case 'w':
	    if (optarg) wsext = optarg; break;
        case '?':
        case 'h':
            print_help(); 
            exit(0);
        };
    }
    
    if (optind < argc) {
	vmimages = (vmimg_t**)malloc((argc - optind) * sizeof(vmimg_t*));
	assert(vmimages);

	while (optind < argc) {
	    vmimages[numimg] = new vmimg_t(argv[optind++]);
	    if (vmimages[numimg]->init())
		numimg++;
	}
    }


    // all VMs read in--now do the calculations...

    unsigned long long comp_duplicates = 0, 
	comp_shareable = 0, 
	act_duplicates = 0, 
	act_shareable = 0, 
	comp_total = 0, 
	pages = 0, 
	zero = 0,
	balloon = 0,
	active = 0;

    unsigned long long comp_duplicates_size = 0,
	comp_shareable_size = 0,
	comp_total_size = 0;

    for (unsigned vmidx = 0; vmidx < numimg; vmidx++) {
	vmimg_t * vm = vmimages[vmidx];
	assert(vm);
	    
	for (unsigned pg = 0; pg < vm->get_pages(); pg++)
	{
	    // now scan...
	    if (vm->pages[pg].is_zero_page()) {
		vm->zeropages++;
	    } 
	    else if (vm->pages[pg].is_balloon_page()) {
		if (verbose) printf("VM%d pg %d (%lx) ballooned\n", vmidx, pg, pg * 4096);
		vm->balloonpages++;
	    }
	    else {
		// first propagate all active bits, except for zero pages...
		for (unsigned vmidx2 = 0; vmidx2 < numimg; vmidx2++)
		    for (unsigned pg2 = 0; pg2 < vmimages[vmidx2]->get_pages(); pg2++)
			if (vm->pages[pg].hash == vmimages[vmidx2]->pages[pg2].hash)
			    vm->pages[pg].active |= vmimages[vmidx2]->pages[pg2].active;
		
		if (vm->pages[pg].active) {
		    vm->activepages++;
		    if (verbose) printf("VM%d pg %d active\n", vmidx, pg);
		}
		else {
		    vm->comppages++;
		    vm->compsize += vm->pages[pg].compressed;
		}
	    
		// try to find page in other page sets (including own)
		if (vm->pages[pg].shared == 0) {
		    for (unsigned vmidx2 = vmidx; vmidx2 < numimg; vmidx2++)
			for (unsigned pg2 = ((vmidx == vmidx2) ? pg + 1 : 0); 
			     pg2 < vmimages[vmidx2]->get_pages(); pg2++)
			{
			    if ((vmimages[vmidx2]->pages[pg2].shared == 0) && 
				vm->pages[pg].hash == vmimages[vmidx2]->pages[pg2].hash) 
			    {
				// duplicate found
				vm->pages[pg].shared++;
				vmimages[vmidx2]->pages[pg2].shared = -1;
				if (vm->pages[pg].active) {
				    act_duplicates++;
				} else {
				    comp_duplicates++;
				    comp_duplicates_size += vm->pages[pg].compressed;
				}

				if (verbose) 
				    printf("duplicate VM%d, pg %d == VM%d, pg %d (%c) (%lx)\n", vmidx, pg, vmidx2, pg2, vm->pages[pg].active ? 'A' : 'C', vm->pages[pg].hash);
			    }
			}
		    if (vm->pages[pg].shared > 0) {
			if (vm->pages[pg].active) {
			    act_shareable++;
			} else {
			    comp_shareable += 1;
			    comp_shareable_size += vm->pages[pg].compressed;
			}
			if (verbose)
			    printf("VM%d pg %d shared %d\n", vmidx, pg, vm->pages[pg].shared);
		    }
		}
	    }
		
	}
	printf("VM %s:\tsize=%d, pages=%d, zero=%d (%f), balloon=%d (%f), active=%d, comppages=%d, compsize=%d (comp ratio=%f), overall ratio: %f\n",
	       vm->filename, vm->get_size(), vm->get_pages(),
	       vm->zeropages, (double)vm->zeropages / (double)vm->get_pages(), 
	       vm->balloonpages, (double)vm->balloonpages / (double)vm->get_pages(),
	       vm->activepages,
	       vm->comppages, vm->compsize, (double)vm->compsize / ((double)vm->comppages * pagesize),
	       (double)vm->compsize / (double)vm->get_size());

	zero += vm->zeropages;
	balloon += vm->balloonpages;
	pages += vm->get_pages();
	active += vm->activepages;
	comp_total += vm->comppages;
	comp_total_size += vm->compsize;
    }
    printf("\n%27s%12s%12s%12s\n", "total", "unshared", "shared", "duplicates");
    printf("%-15s%12Ld%12Ld%12Ld%12Ld\n", "active pg", active, 
	   active - act_shareable - act_duplicates,
	   act_shareable, act_duplicates);
    printf("%-15s%12Ld%12Ld%12Ld%12Ld\n", "compressed pg", comp_total, 
	   comp_total - comp_shareable - comp_duplicates,
	   comp_shareable, comp_duplicates);
    printf("%-15s%12d\n", "zero pg", zero);
    printf("%-15s%12d\n", "balloon pg", balloon);

    printf("\n%-15s%12Ld%12Ld%12Ld%12Ld\n", "active (size)", active * pagesize, 
	   (active - act_shareable - act_duplicates) * pagesize,
	   act_shareable * pagesize, act_duplicates * pagesize);

    printf("%-15s%12Ld%12Ld%12Ld%12Ld\n", "compr (size)", comp_total_size, 
	   comp_total_size - comp_shareable_size - comp_duplicates_size,
	   comp_shareable_size, comp_duplicates_size);
    printf("%-15s%12Ld%12Ld%12Ld%12Ld\n", "uncompr (size)", comp_total * pagesize, 
	   (comp_total - comp_shareable - comp_duplicates) * pagesize,
	   comp_shareable * pagesize, comp_duplicates * pagesize);
    printf("%-15s%12d\n", "zero (size)", zero * pagesize);
    printf("%-15s%12d\n", "balloon (size)", balloon * pagesize);
	   
    printf("memory: compressable %Ld (compressed %Ld), uncompressed %Ld, zero %Ld, balloon %Ld, duplicates %Ld\n", 
	   (comp_total - comp_duplicates) * pagesize, comp_total_size - comp_duplicates_size,
	   (active - act_duplicates) * pagesize, zero * pagesize, balloon * pagesize,
	   (comp_duplicates + act_duplicates) * pagesize);
    printf("total: %Ld Bytes (ratio: %f)\n", 
	   comp_total_size - comp_duplicates_size + (active - act_duplicates) * pagesize,
	   (double)((comp_total_size - comp_duplicates_size) + (active - act_duplicates) * pagesize) / (double)(pages * pagesize));
}
