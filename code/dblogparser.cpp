
#include <dirent.h> 
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <algorithm>

// Possible error numbers. Returning 0 means success.
#define ERROR_ARGC       1
#define ERROR_DBSIZE     2
#define ERROR_ENTRYSIZE  3
#define ERROR_SVDATASIZE 4
#define ERROR_DIRACCESS  5
#define ERROR_FILEACCESS 6
#define ERROR_OUTACCESS  7
#define ERROR_ALLOC      8

// A couple of constant values.
const int timelen = 20; // "yyyy-mm-dd hh:mm:ss\0"
const char line[]  = "> a] CONSOLE [ (DBDUMP) ";
const char line2[] = "> a] CONSOLE [ (SVDATA) ";
const int linelen = strlen(line);
const int minlen = timelen+linelen;

// We use lots of global vars here. The reason for this is that I'd like
// to clean up the memory correctly while not using RAII paradigm.
int dbsize = 0;
int entrysize = 0;
int** database = NULL;
int svdatasize = 0;
int* svdata = NULL;
char* fname[10000];
int files = 0;
char lastupdate[timelen];

// This is used by std::sort to sort the filenames.
// We skip the first four chars ("gen-") for some microoptimization :)
bool comparator(char* a, char* b)
{
    return (strcmp(a+4,b+4)<0);
}

int function(int argc, char* argv[])
{
    // First, let's process the command line parameters.
    // argv[0] is name of this program.
    // argv[1] is number of entries in the database.
    // argv[2] is size of each entry (in 32-bit integers).
    // argv[3] is size of server data (in 32-bit integers).
    // argv[4] is output file name.
    if (argc<5)
    {
        printf("[ERROR] Usage: %s dbsize entrysize outputfile.\n",argv[0]);
        return ERROR_ARGC;
    }    
    dbsize = atoi(argv[1]);    
    if (dbsize<=10000 || dbsize>2000000000)
    {
        printf("[ERROR] Bad database size (%d).\n",dbsize);
        return ERROR_DBSIZE;
    }
    entrysize = atoi(argv[2]);
    if (entrysize<4 || entrysize>1000)
    {
        printf("[ERROR] Bad entry size (%d).\n",entrysize);
        return ERROR_ENTRYSIZE;
    }
	svdatasize = atoi(argv[3]);
    if (svdatasize<0)
    {
        printf("[ERROR] Bad svdata size (%d).\n",svdatasize);
        return ERROR_SVDATASIZE;
    }	
    char* outname = argv[4];
    
    // Now we create the database in RAM and fill it with zeroes.
    database = new int*[dbsize];
	if (database == NULL)
	{
		printf("[ERROR] Memory allocation error.\n");
		return ERROR_ALLOC;
	}
    for (int entry=0; entry<dbsize; entry++)
    {
        database[entry] = new int[entrysize];
		if (database[entry] == NULL)
		{
			printf("[ERROR] Memory allocation error.\n");
			return ERROR_ALLOC;
		}
        memset(database[entry], 0, sizeof(int)*entrysize);
    }
	svdata = new int[svdatasize];
	if (svdata == NULL)
	{
		printf("[ERROR] Memory allocation error.\n");
		return ERROR_ALLOC;
	}
    
    memset(lastupdate, '\0', timelen);
    memset(fname, (int)NULL, sizeof(char*)*10000);
    
    // Open the working directory.
    DIR* dir = opendir(".");
    if (dir==NULL)
    {
        perror("[ERROR] Couldn't open working directory");
        return ERROR_DIRACCESS;
    }
    
    // Find all log files in this directory.
    while (true)
    {
        // Read the next file.
        struct dirent* direntry = readdir(dir);
        if (direntry == NULL) break;
		
		//printf("Found file: d_name='%s', d_type=%d\n",direntry->d_name,direntry->d_type);		
        // Ignore directories, system files and other weird shit.
        //if (direntry->d_type != DT_REG) continue; // screws everything up, don't uncomment
        
		// Is the length of the file name matching?
		if (strlen(direntry->d_name) != 16) continue;
		
        // Is it a "gen-*.log" file?
        if (strncmp(direntry->d_name+00,"gen-",4)!=0 ||
            strncmp(direntry->d_name+12,".log",4)!=0) continue;
        
        // Yep, it's all good. Store this file name.
        //printf("Found file '%s'\n",direntry->d_name);
        fname[files] = new char[300];
		if (fname[files] == NULL)
		{
			printf("[ERROR] Memory allocation error.\n");
			return ERROR_ALLOC;
		}
        strcpy(fname[files], direntry->d_name);
        files++;
    }
    closedir(dir);
        
    // We need to process all the logs in the right order.
    // Thankfully, their filenames allow use to simply sort them.
    std::sort(fname, fname+files, comparator);
    //printf("Sorted the logfiles.\n");
    
    // We are now ready to process each log file.    
    for (int i=0; i<files; i++)
    {
        // Open the file.
       // printf("Parsing file '%s'...\n",fname[i]);
        FILE* logfile = fopen(fname[i], "rt");
        if (logfile==NULL)
        {
            printf("[ERROR] Couldn't access file '%s'.\n",fname[i]);
            return ERROR_FILEACCESS;
        }
        char buf[1001];
        while (true)
        {
            // Get next line in the log file.
            if (!fgets(buf,1000,logfile)) break;
            int len = strlen(buf);
			
            // Is the line too short?
            if (len < minlen) continue;
			            
            // Is this a dbdump line?
            if (strncmp(buf+timelen,line,linelen)==0)
			{            
				// This is a valid dbdump line.
				strncpy(lastupdate, buf, timelen-1); // don't write the space char at the end
				int entry;
				char* oldptr = buf+minlen;
				//printf("Searching for dbdump values in string '%s'...\n",oldptr);
				char* ptr;
				entry = strtoul(oldptr, &ptr, 16);
				
				// Note: this 'while' loop can stop before we read as many as
				// 'entrysize' values if we reach the end of the line first.
				int index = 0;
				while (index<entrysize)
				{
					char* oldptr = ptr;
					int value = strtoul(oldptr, &ptr, 16);
					if (oldptr==ptr) break;
					database[entry][index] = value;
					index++;
				}
			}
			// It is not. But is this a svdata line?
            else if (strncmp(buf+timelen,line2,linelen)==0)
			{
				// This is a valid svdata line.
				strncpy(lastupdate, buf, timelen-1); // don't write the space char at the end
				char* oldptr = buf+minlen;
				//printf("Searching for svdata values in string '%s'...\n",oldptr);
				char* ptr;
				svdata[0] = strtoul(oldptr, &ptr, 16);
				int index = 1;
				while (index<svdatasize)
				{
					char* oldptr = ptr;
					int value = strtoul(oldptr, &ptr, 16);
					if (oldptr==ptr) break;
					svdata[index] = value;
					index++;
				}
			}
        }
        fclose(logfile);
    }
    
	printf("[SUCCESS] dblogscan: %d logfiles parsed; last update %s.\n",files,lastupdate);

    // Now we dump our database into the output file.
    FILE* fout = fopen(outname, "wb");
    if (fout == NULL)
    {
        printf("[ERROR] Couldn't create '%s'.\n",outname);
        return ERROR_OUTACCESS;
    }
    
    // First, we write the time of the date and time of last update.
    // The date/time string itself takes 19 characters but we'll write 20 instead.
    // The reason behind this is that the entire rest of the database is going
    // to be a set of integers. It would be silly to break the 4-byte 'align'.
    fwrite(lastupdate, timelen, 1, fout);
	
	// The next three integers are database size, entry size, and svdata size.
	fwrite(&dbsize, sizeof(int), 1, fout);
	fwrite(&entrysize, sizeof(int), 1, fout);
	fwrite(&svdatasize, sizeof(int), 1, fout);
	
	// The next svdatasize integers are (surprisingly) the actual server data.
	fwrite(svdata, sizeof(int), svdatasize, fout);
	
	// And finally, actual data.
	for (int entry=0; entry<dbsize; entry++)
	{
		fwrite(database[entry], sizeof(int), entrysize, fout);
		//printf("Name: %c%c%c%c..\n",database[entry][0]&0xFF, (database[entry][0]>>8)&0xFF, (database[entry][0]>>16)&0xFF, (database[entry][0]>>24)&0xFF);
	}	
    fclose(fout);
	
	printf("[SUCCESS] dblogscan: database saved on disk.\n");
	
    return 0;
}

int main (int argc, char* argv[])
{
    // We run the main function.
    int exitcode = function(argc, argv);
    
    // Regardless of whether the function succeedes or not
    // we have to clean up the memory correctly.
    for (int entry=0; entry<dbsize; entry++)
    {
        // Extra check.
        if (database[entry] != NULL) delete database[entry];
    }
    if (database != NULL) delete database;
	if (svdata != NULL) delete svdata;
    for (int f=0; f<files; f++)
    {
        // Extra check.
        if (fname[f] != NULL) delete fname[f];
    }
    
    // Return the exit code.
    return exitcode;
}


