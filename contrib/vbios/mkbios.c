#include <stdio.h>

#define VGABASE     0x000C0000
#define ROMBASE     0x000F0000
#define BIOS_SIZE   0x00400000

int main(int argc, char **argv)
{
    FILE *vgain, *romin, *biosout;
    int i;
    if(argc != 4) {
        printf("Usage: %s vgabios rombios outfile\n\r", argv[0]);
	return 0;
    }
    if( (vgain = fopen(argv[1], "rb")) == NULL) {
        perror("error opening vgabios");
	return 1;
    }
    if( (romin = fopen(argv[2], "rb")) == NULL) {
        perror("error opening rombios");
	return 1;
    }
    if( (biosout = fopen(argv[3], "wb")) == NULL) {
        perror("error writing bios");
	return 1;
    }
    // fill up with zeros
    for(i=0;i<VGABASE;i++)
    	fputc(0, biosout);

    // append vgabios
    while(!feof(vgain)) {
        fputc(fgetc(vgain), biosout);
	i++;
    }
    // fill up with zeros
    for(;i<ROMBASE;i++)
        fputc(0, biosout);

    // append rombios
    while(!feof(romin)) {
        fputc(fgetc(romin), biosout);
	i++;
    }
    // fill up again
    for(;i<BIOS_SIZE;i++)
        fputc(0, biosout);

    fclose(vgain);
    fclose(romin);
    fclose(biosout);
    return 0;
}
