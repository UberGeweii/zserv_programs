
#include <dirent.h> 
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>

#define LUMPOFFSET 0x00
#define LUMPSIZE   0x04
#define LUMPNAME   0x08

#define NUMLUMPS   0x04
#define DIRPTR     0x08

typedef unsigned char BYTE;

union SEGINT // 'segmented' integer
{
    int value;
    BYTE frac[4];
    SEGINT(int value1) : value(value1) { }
    SEGINT(BYTE b1, BYTE b2, BYTE b3, BYTE b4)
    {
    	frac[0] = b1;
    	frac[1] = b2;
    	frac[2] = b3;
    	frac[3] = b4;
    }
};

int ReadInt(BYTE* wad1, int offset)
{
	SEGINT l(wad1[offset+0],wad1[offset+1],wad1[offset+2],wad1[offset+3]);
	return l.value;
}

void WriteInt(BYTE* wad1, int offset, SEGINT segint)
{
    wad1[offset+0] = segint.frac[0];
    wad1[offset+1] = segint.frac[1];
    wad1[offset+2] = segint.frac[2];
    wad1[offset+3] = segint.frac[3];
}

BYTE* wad;
const int maxwadsize = 128*1024*1024;

int function(char* filename)
{    
    FILE* foldwad = fopen(filename, "rb");
    if (foldwad == NULL)
    {
        printf("[ERROR] blaaah.\n");
        return 1;
    }
	int oldsize = fread(wad, sizeof(BYTE), maxwadsize, foldwad);
	fclose(foldwad);
	
	printf("[SUCCESS] Loaded %d bytes (%.2lf MB) of input wad.\n",oldsize,((double)oldsize)/1024.0/1024.0);
    
	int lumps = ReadInt(wad, LUMPSIZE);
	int pdir  = ReadInt(wad, LUMPNAME);
	printf("[DEBUG] Total lumps: %d, directory ptr: %d\n",lumps,pdir);
	//system("PAUSE");
	
	// Let's find the MAP01 lump.
	char lumpname[9]; lumpname[8] = '\0';
	for (int lumpid=0; lumpid<lumps; lumpid++)
	{
		strncpy(lumpname,(char*)(&wad[pdir + lumpid*16 + LUMPNAME]), 8);
		printf("[DEBUG] Found lump '%-8s': offset %8d, size %d\n",lumpname,ReadInt(wad,pdir + lumpid*16 + LUMPOFFSET),ReadInt(wad,pdir + lumpid*16 + LUMPSIZE));
    }    
    return 0;
}

int main (int argc, char* argv[])
{
    // Allocate memory for the wad
    wad = new BYTE[maxwadsize];
    if (wad == NULL)
    {
        printf("[ERROR] penis.\n");
        return 1;
    }
    
    // Run the function
    int exitcode = function("zdcity2_db1.wad");
    //int exitcode = function("zdcity2alpha_v04c1.wad");
    
    // Deallocate the memory
    delete wad;
    
    // Return the exit code.
    printf("Exitcode = %d\n",exitcode); system("PAUSE");
    return exitcode;
}






