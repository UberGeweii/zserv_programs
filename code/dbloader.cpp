
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

#define ERROR_ALLOCATION     10
#define ERROR_ARGC           11
#define ERROR_BROKENMAP      12
#define ERROR_MULTIPLEMAP    13
#define ERROR_NOMAP          14
#define ERROR_ACCESSOLD      15
#define ERROR_ACCESSBIN      16
#define ERROR_ACCESSNEW      17
#define ERROR_INITNOTFOUND   20
#define ERROR_BADDBSIZE      21
#define ERROR_BADENTRYSIZE   22
#define ERROR_BADSVDATASIZE  23
#define ERROR_DBINITNOTFOUND 24
#define ERROR_NOTENOUGHINIT  25

BYTE* wad;
const int maxwadsize = 128*1024*1024;

int function(int argc, char* argv[])
{
    // argv[0] is name of this program.
    // argv[1] is name of the original wad file.
    // argv[2] is name of the database binary file.
    // argv[3] is name of the output wad file.
    if (argc<4)
    {
        printf("[ERROR] Usage: %s oldwadname databasename newwadname.\n",argv[0]);
        return ERROR_ARGC;
    }
    
    FILE* foldwad = fopen(argv[1], "rb");
    if (foldwad == NULL)
    {
        printf("[ERROR] Couldn't read file '%s'.\n",argv[1]);
        return ERROR_ACCESSOLD;
    }
	int oldsize = fread(wad, sizeof(BYTE), maxwadsize, foldwad);
	fclose(foldwad);
	    
	int lumps = ReadInt(wad, LUMPSIZE);
	int pdir  = ReadInt(wad, LUMPNAME);
	printf("[SUCCESS] dbloader: loaded %.2lf MB (total lumps: %d).\n",((double)oldsize)/1024.0/1024.0,lumps);
	
	// Let's find the MAP01 lump.
	int maplumpid;
	for (maplumpid=0; maplumpid<lumps; maplumpid++)
	{
		char* lumpname = (char*)(&wad[pdir + maplumpid*16 + LUMPNAME]);
		if (strcmp(lumpname, "MAP01")==0)
		{
            // We found MAP01.. But is it the only MAP01 in the file?
            for (int otherid=maplumpid+1; otherid<lumps; otherid++)
            {
                char* otherlumpname = (char*)(&wad[pdir + otherid*16 + LUMPNAME]);
                if (strcmp(otherlumpname, "MAP01")==0)
                {
                    printf("[ERROR] Multiple 'MAP01' lumps were found.\n");
                    return ERROR_MULTIPLEMAP;
                }
            }
            // Yep, it is. But is the map structure okay?
            if (maplumpid+12 > lumps
                || strncmp((char*)(&wad[pdir + (maplumpid+ 1)*16 + LUMPNAME]), "THINGS"  , 8) != 0
                || strncmp((char*)(&wad[pdir + (maplumpid+ 2)*16 + LUMPNAME]), "LINEDEFS", 8) != 0
                || strncmp((char*)(&wad[pdir + (maplumpid+ 3)*16 + LUMPNAME]), "SIDEDEFS", 8) != 0
                || strncmp((char*)(&wad[pdir + (maplumpid+ 4)*16 + LUMPNAME]), "VERTEXES", 8) != 0
                || strncmp((char*)(&wad[pdir + (maplumpid+ 5)*16 + LUMPNAME]), "SEGS"    , 8) != 0
                || strncmp((char*)(&wad[pdir + (maplumpid+ 6)*16 + LUMPNAME]), "SSECTORS", 8) != 0
                || strncmp((char*)(&wad[pdir + (maplumpid+ 7)*16 + LUMPNAME]), "NODES"   , 8) != 0
                || strncmp((char*)(&wad[pdir + (maplumpid+ 8)*16 + LUMPNAME]), "SECTORS" , 8) != 0
                || strncmp((char*)(&wad[pdir + (maplumpid+ 9)*16 + LUMPNAME]), "REJECT"  , 8) != 0
                || strncmp((char*)(&wad[pdir + (maplumpid+10)*16 + LUMPNAME]), "BLOCKMAP", 8) != 0
                || strncmp((char*)(&wad[pdir + (maplumpid+11)*16 + LUMPNAME]), "BEHAVIOR", 8) != 0
                )
            {
                printf("[ERROR] Structure of map MAP01 seems to be broken.\n");
                return ERROR_BROKENMAP;
            }
            // Looks ok. Then we 'break' from this loop.
            break;
        }
    }
    if (maplumpid==lumps)
    {
        printf("[ERROR] Couldn't find lump 'MAP01'.\n");
        return ERROR_NOMAP;
    }
    printf("[SUCCESS] dbloader: 'MAP01' lump found (index %d at %d).\n",maplumpid,ReadInt(wad,pdir + maplumpid*16 + LUMPOFFSET));
	
	// WAD header takes 12 bytes. MAP01 lump will start directly after the header.
	// This means that we have shift each wad lump by this value in the directory.
	// Let's also calculate how much memory the entire map data takes.
    int mapoffset = ReadInt(wad, pdir + maplumpid*16 + LUMPOFFSET);
    int delta = mapoffset - 12;
    int mapsize = 0;
    for (int mapdata=0; mapdata<12; mapdata++)
    {
        int oldoffset = ReadInt(wad, pdir + (maplumpid+mapdata)*16 + LUMPOFFSET);
        //printf("[DEBUG] old offset: %d, ",oldoffset);
        WriteInt(wad, pdir + (maplumpid+mapdata)*16 + LUMPOFFSET, oldoffset - delta);
        //printf("new offset: %d\n",ReadInt(wad, pdir + (maplumpid+mapdata)*16 + LUMPOFFSET));
        mapsize += ReadInt(wad, pdir + (maplumpid+mapdata)*16 + LUMPSIZE);
        //printf("[DEBUG] Adding %d to mapsize...\n",ReadInt(wad, pdir + (maplumpid+mapdata)*16 + LUMPSIZE));
    }
    //printf("[DEBUG] Delta offset: %d, mapsize: %d\n",delta,mapsize);
        
    // Now that the directory lump is fixed we need to move the actual mapdata too.
    memmove(&wad[12], &wad[mapoffset], (size_t)mapsize);
    
    // Alright, so now we also have to move the directory too.
    // Mind that we only need these 12 lumps, not the whole directory.
    memmove(&wad[12+mapsize], &wad[pdir + maplumpid*16], (size_t)16*12);
    
    // And finally we fix the header.
    WriteInt(wad, NUMLUMPS, 12);
    WriteInt(wad, DIRPTR, 12+mapsize);
    
    printf("[SUCCESS] dbloader: new wad constructed.\n");
    
    
    // Alright, time to open the database.
    FILE* fbin = fopen(argv[2], "rb");
    if (fbin == NULL)
    {
        printf("[ERROR] Couldn't read file '%s'.\n",argv[2]);
        return ERROR_ACCESSBIN;
    }
    // The first 20 bytes are date/time of last update. We don't care about that.
    fseek(fbin, 20, SEEK_SET);
    
    // The next 3 integers are database size, entry size, and svdata size.
	int dbsize;     fread(&dbsize,     sizeof(int), 1, fbin);
	int entrysize;  fread(&entrysize,  sizeof(int), 1, fbin);
	int svdatasize; fread(&svdatasize, sizeof(int), 1, fbin); 
        
    // Now here's the fun part - we're hacking into BEHAVIOUR lump of the new wad.
    int pbeh    = ReadInt(wad, 12+mapsize+11*16+LUMPOFFSET);
    int behsize = ReadInt(wad, 12+mapsize+11*16+LUMPSIZE);
    //printf("BEHAVIOR starts with: '%c%c%c%c'\n",wad[pbeh+0],wad[pbeh+1],wad[pbeh+2],wad[pbeh+3]);
    
    // Slowly and painfully we are searching for "AINI" strings in the lump.
    bool initfound = false;
    int aini = 0;
    for (aini=0; aini<behsize-4 && !initfound; aini++)
    {
        if (strncmp((char*)(&wad[pbeh+aini]), "AINI", 4)==0)
        {
            //printf("I found some 'AINI' at 0x%x...\n",i);
            if (aini+48>behsize) break; // error
            if (strncmp((char*)(&wad[pbeh+aini+12]), "SERVDATAINIT", 12)==0)
            {
                initfound = true;
                break;
                //printf("Holy shit I found it at 0x%x!!!\n",i);
            }
        }
    }
    
    // Lotsa checks!
    if (initfound == false)
    {
        printf("[ERROR] Failed to find 'AINI' + 'SERVDATAINIT'.\n");
        fclose(fbin);
        return ERROR_INITNOTFOUND;
    }
    if (ReadInt(wad, pbeh+aini+24) != dbsize)
    {
        printf("[ERROR] Database size in binary file: %d, in ACS: %d.\n",dbsize,ReadInt(wad, pbeh+aini+24));
        fclose(fbin);
        return ERROR_BADDBSIZE;
    }
    if (ReadInt(wad, pbeh+aini+28) != entrysize)
    {
        printf("[ERROR] Entry size in binary file: %d, in ACS: %d.\n",entrysize,ReadInt(wad, pbeh+aini+28));
        fclose(fbin);
        return ERROR_BADENTRYSIZE;
    }
    if (ReadInt(wad, pbeh+aini+32) != svdatasize)
    {
        printf("[ERROR] Server data size in binary file: %d, in ACS: %d.\n",svdatasize,ReadInt(wad, pbeh+aini+32));
        fclose(fbin);
        return ERROR_BADSVDATASIZE;
    }
    
    // Ok, we can safely upload the server data block now.
    fread(&wad[pbeh+aini+36], sizeof(int), svdatasize, fbin); 
    
    // A few more checks!
    int aini2 = aini+36+svdatasize*4;
    if (strncmp((char*)(&wad[pbeh+aini2]), "AINI", 4)!=0)
    {
        printf("[ERROR] Database init not found RIGHT after general init.\n");
        fclose(fbin);
        return ERROR_DBINITNOTFOUND;
    }
    if (ReadInt(wad, pbeh+aini2+4) < dbsize*entrysize*4)
    {
        printf("[ERROR] Not enough memory allocated in BEHAVIOR (has %d, needs %d).\n",ReadInt(wad, pbeh+aini2+4),dbsize*entrysize);
        fclose(fbin);
        return ERROR_NOTENOUGHINIT;
    }
    
    // FINALLY we upload the database into the BEHAVIOR lump.
    fread(&wad[pbeh+aini2+12], sizeof(int), dbsize*entrysize*4, fbin);
    fclose(fbin);
    
    printf("[SUCCESS] dbloader: database uploaded into BEHAVIOR lump.\n");
    
    
    // Time to output the wad. The size consists of header + mapdata + new directory.
    FILE* fnewwad = fopen(argv[3], "wb");
    if (fnewwad == NULL)
    {
        printf("[ERROR] Couldn't write file '%s'.\n",argv[3]);
        return ERROR_ACCESSNEW;
    }
	fwrite(wad, sizeof(BYTE), 12+mapsize+16*12, fnewwad);
	fclose(fnewwad);
    
    printf("[SUCCESS] dbloader: optional wad saved on disk.\n");
    
    return 0;
}

int main (int argc, char* argv[])
{
    // Allocate memory for the wad
    wad = new BYTE[maxwadsize];
    if (wad == NULL)
    {
        printf("[ERROR] Failed to allocate %d MB to load the wad.\n",maxwadsize/1024/1024);
        return ERROR_ALLOCATION;
    }
    
    // Run the function
    int exitcode = function(argc, argv);
    
    // Deallocate the memory
    delete wad;
    
    // Return the exit code.
    //printf("Exitcode = %d. ",exitcode); system("PAUSE");
    return exitcode;
}






