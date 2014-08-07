// os345fat.c - file management system
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the CS345 projects.          **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************
//
//		11/19/2011	moved getNextDirEntry to P6
//
// ***********************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include "os345.h"
#include "os345fat.h"

// ***********************************************************************
// ***********************************************************************
//	functions to implement in Project 6
//
int fmsCloseFile(int);
int fmsDefineFile(char*, int);
int fmsDeleteFile(char*);
int fmsOpenFile(char*, int);
int fmsReadFile(int, char*, int);
int fmsSeekFile(int, int);
int fmsWriteFile(int, char*, int);

// ***********************************************************************
// ***********************************************************************
//	Support functions available in os345p6.c
//
extern int fmsGetDirEntry(char* fileName, DirEntry* dirEntry);
extern int fmsGetNextDirEntry(int *dirNum, char* mask, DirEntry* dirEntry, int dir);

extern int fmsMount(char* fileName, void* ramDisk);

extern void setFatEntry(int FATindex, unsigned short FAT12ClusEntryVal, unsigned char* FAT);
extern unsigned short getFatEntry(int FATindex, unsigned char* FATtable);

extern int fmsMask(char* mask, char* name, char* ext);
extern void setDirTimeDate(DirEntry* dir);
extern int isValidFileName(char* fileName);
extern void printDirectoryEntry(DirEntry*);
extern void fmsError(int);

extern int fmsReadSector(void* buffer, int sectorNumber);
extern int fmsWriteSector(void* buffer, int sectorNumber);

// ***********************************************************************
// ***********************************************************************
// fms variables
//
// RAM disk
unsigned char RAMDisk[SECTORS_PER_DISK * BYTES_PER_SECTOR];

// File Allocation Tables (FAT1 & FAT2)
unsigned char FAT1[NUM_FAT_SECTORS * BYTES_PER_SECTOR];
unsigned char FAT2[NUM_FAT_SECTORS * BYTES_PER_SECTOR];

char dirPath[128];							// current directory path
FDEntry OFTable[NFILES];					// open file table

extern bool diskMounted;					// disk has been mounted
extern TCB tcb[];							// task control block
extern int curTask;							// current task #

extern int openFileCount;
extern bool OFTableTaken[];


// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
//	Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor)
{
	if(fileDescriptor>31 || fileDescriptor < 0){
		return ERR52;
	}
	if(!OFTableTaken[fileDescriptor]){
		return ERR63;
	}
	// Flushes transaction buffer
	// Update directory if file altered
	// End-of-file
	// Creation date/time
	OFTable[fileDescriptor].name[0] = 0;


	return 0;
} // end fmsCloseFile



// ***********************************************************************
// ***********************************************************************
// If attribute=DIRECTORY, this function creates a new directory
// file directoryName in the current directory.
// The directory entries "." and ".." are also defined.
// It is an error to try and create a directory that already exists.
//
// else, this function creates a new file fileName in the current directory.
// It is an error to try and create a file that already exists.
// The start cluster field should be initialized to cluster 0.  In FAT-12,
// files of size 0 should point to cluster 0 (otherwise chkdsk should report an error).
// Remember to change the start cluster field from 0 to a free cluster when writing to the
// file.
//
// Return 0 for success, otherwise, return the error number.
//
int fmsDefineFile(char* fileName, int attribute)
{
	// ?? add code here
	printf("\nfmsDefineFile Not Implemented");

	return ERR72;
} // end fmsDefineFile



// ***********************************************************************
// ***********************************************************************
// This function deletes the file fileName from the current directory.
// The file name should be marked with an "E5" as the first character and the chained
// clusters in FAT 1 reallocated (cleared to 0).
// Return 0 for success; otherwise, return the error number.
//
int fmsDeleteFile(char* fileName)
{
	// ?? add code here
	printf("\nfmsDeleteFile Not Implemented");

	return ERR61;
} // end fmsDeleteFile



// ***********************************************************************
// ***********************************************************************
// This function opens the file fileName for access as specified by rwMode.
// It is an error to try to open a file that does not exist.
// The open mode rwMode is defined as follows:
//    0 - Read access only.
//       The file pointer is initialized to the beginning of the file.
//       Writing to this file is not allowed.
//    1 - Write access only.
//       The file pointer is initialized to the beginning of the file.
//       Reading from this file is not allowed.
//    2 - Append access.
//       The file pointer is moved to the end of the file.
//       Reading from this file is not allowed.
//    3 - Read/Write access.
//       The file pointer is initialized to the beginning of the file.
//       Both read and writing to the file is allowed.
// A maximum of 32 files may be open at any one time.
// If successful, return a file descriptor that is used in calling subsequent file
// handling functions; otherwise, return the error number.
//

void createFDEntry(FDEntry* fdEntry, DirEntry dirEntry, int rwMode){
	int error;
	strncpy(&(fdEntry->name), (char*)&(dirEntry.name), 8);
	strncpy(&(fdEntry->extension), (char*)&(dirEntry.extension), 3);
	fdEntry->attributes = dirEntry.attributes;
	fdEntry->directoryCluster = CDIR;
	fdEntry->startCluster = dirEntry.startCluster;
	fdEntry->currentCluster = dirEntry.startCluster;
	fdEntry->fileSize = (rwMode==1) ? 0 : dirEntry.fileSize;
	fdEntry->pid = curTask;
	fdEntry->mode = rwMode;
	fdEntry->flags = 0;
	fdEntry->fileIndex = (rwMode==2) ? dirEntry.fileSize : 0;

	// i am not sure if i have to do this here..
	// short nextCluster;
	// fdEntry->currentCluster = fdEntry->startCluster;
	// while ((nextCluster = getFatEntry(fdEntry->currentCluster, FAT1)) != FAT_EOC)
	//          fdEntry->currentCluster = nextCluster;
	// if ((error = fmsReadSector(fdEntry->buffer,fdEntry->currentCluster))) return error;
}

int fmsOpenFile(char* fileName, int rwMode)
{
	int error = 0;
	int index = 0;
	int i;
	char mask[20];
	strcpy(mask, "*.*");
	DirEntry dirEntry;

	if(!isValidFileName(fileName)){
		return ERR50;
	}

	char name[13];
	for(i=0;i<13;i++){
		name[i] = ' ';
	}
	int nameCount = 0;
	int extCount = 0;
	bool isName = 1;
	//checking name is valid
	for(i=0;i<14;i++){
		if(fileName[i]=='\0'){
			break;
		}
		if(fileName[i]=='.'){
			nameCount = 8;
			isName = 0;
			continue;
		}
		fileName[i] = toupper(fileName[i]);
		if(isName){
			if(nameCount >= 8){
				return ERR50;
			}
			name[nameCount++] = fileName[i];
		}else{
			if(extCount >= 3){
				return ERR50;
			}
			name[nameCount++] = fileName[i];
			extCount++;
		}
	}
	while (1)
	{
		error = fmsGetNextDirEntry(&index, mask, &dirEntry, CDIR);
		if (error)
		{
			if (error != ERR67) fmsError(error);
			if (error == ERR67) return ERR61;
			break;
		}

		//check if file or dir
		if(!(dirEntry.attributes & 0x10) && !(dirEntry.attributes & 0x08) ){
			bool sameName = 1;
			// printf("\n%s filename \n%s", "not a directory or volume", dirEntry.name);
			for(i=0;i<12;i++){
				if(name[i]!=dirEntry.name[i]){
					sameName = 0;
				}
			}
			if(sameName){
				// printf("\n%s", "found file!");
				if(openFileCount>=32){
					return ERR70;
				}
				// check if file is already open
				//create FDEntry
				FDEntry fdEntry;
				// if(error=createFDEntry(&fdEntry, dirEntry, rwMode)) return error;
				createFDEntry(&fdEntry, dirEntry, rwMode);
				int fileDescriptor = 0;
				for(i=0;i<NFILES;i++){
					if(OFTable[i].name[0] == 0){
						fileDescriptor = i;
						OFTableTaken[i] = 1;
						openFileCount++;
						break;
					}
				}
				OFTable[fileDescriptor] = fdEntry;
				return fileDescriptor;
			}
		}
		SWAP;
	}
	return ERR61;
} // end fmsOpenFile



// ***********************************************************************
// ***********************************************************************
// This function reads nBytes bytes from the open file specified by fileDescriptor into
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// After each read, the file pointer is advanced.
// Return the number of bytes successfully read (if > 0) or return an error number.
// (If you are already at the end of the file, return EOF error.  ie. you should never
// return a 0.)
//
int fmsReadFile(int fileDescriptor, char* buffer, int nBytes)
{
	int error;
	short nextCluster;
	if(OFTable[fileDescriptor].name[0]==0){
		return ERR63;
	}
	if(OFTable[fileDescriptor].mode==1){
		return ERR84;
	}
	int fileIndex = OFTable[fileDescriptor].fileIndex;
	int fileSize = OFTable[fileDescriptor].fileSize; 
	printf("\n%d  %d", fileIndex, fileSize);
	if(fileSize==fileIndex){
		return ERR66;
	}
	//check for other error conditions here

	int fileLeft = fileSize-fileIndex;
	if(nBytes > fileLeft){
		nBytes = fileLeft;
	}

	int sectorIndex = fileIndex % BYTES_PER_SECTOR;
	int bytesLeft = nBytes;
	int indexIntoBuffer = 0;
	while(bytesLeft){
		if ((error = fmsReadSector(OFTable[fileDescriptor].buffer,C_2_S(OFTable[fileDescriptor].currentCluster)))) return error;
		if(bytesLeft + sectorIndex > BYTES_PER_SECTOR){
			//read all bytes left in this sector, go to next sector
			memcpy(buffer, &OFTable[fileDescriptor].buffer[sectorIndex], BYTES_PER_SECTOR-sectorIndex);
			buffer+=(BYTES_PER_SECTOR-sectorIndex);
			bytesLeft-=(BYTES_PER_SECTOR-sectorIndex);
			sectorIndex = 0;
			if ((nextCluster = getFatEntry(OFTable[fileDescriptor].currentCluster, FAT1)) != FAT_EOC){
			         OFTable[fileDescriptor].currentCluster = nextCluster;
			}else{
				printf("\n%s", "this should not happen");
				return ERR66;
			}
		}else{
			memcpy(buffer, &OFTable[fileDescriptor].buffer[sectorIndex], bytesLeft);
			// printf("%s\n", buffer);
			buffer+=bytesLeft;
			sectorIndex+=bytesLeft;
			bytesLeft = 0;
		}
	}
	OFTable[fileDescriptor].fileIndex+=nBytes;
	return nBytes;

} // end fmsReadFile



// ***********************************************************************
// ***********************************************************************
// This function changes the current file pointer of the open file specified by
// fileDescriptor to the new file position specified by index.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// The file position may not be positioned beyond the end of the file.
// Return the new position in the file if successful; otherwise, return the error number.
//
int fmsSeekFile(int fileDescriptor, int index)
{
	// ?? add code here
	printf("\nfmsSeekFile Not Implemented");

	return ERR63;
} // end fmsSeekFile



// ***********************************************************************
// ***********************************************************************
// This function writes nBytes bytes to the open file specified by fileDescriptor from
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// Writing is always "overwriting" not "inserting" in the file and always writes forward
// from the current file pointer position.
// Return the number of bytes successfully written; otherwise, return the error number.
//
int fmsWriteFile(int fileDescriptor, char* buffer, int nBytes)
{
	printf("\n%s %d", buffer, nBytes);
	if(OFTable[fileDescriptor].name[0]==0){
		return ERR63;
	}
	if(OFTable[fileDescriptor].mode==0){
		return ERR84;
	}
	int error;
	short nextCluster;

	int fileIndex = OFTable[fileDescriptor].fileIndex;
	int sectorIndex = fileIndex % BYTES_PER_SECTOR;
	int bytesLeft = nBytes;
	while(bytesLeft){

		//read into buffer for writing out later
		if ((error = fmsReadSector(OFTable[fileDescriptor].buffer,C_2_S(OFTable[fileDescriptor].currentCluster)))) return error;

		if(nBytes > BYTES_PER_SECTOR - sectorIndex){
			// i need to write to the next cluster, write what i can first.. 
			memcpy(&OFTable[fileDescriptor].buffer[sectorIndex],buffer,BYTES_PER_SECTOR-sectorIndex);
			buffer+=(BYTES_PER_SECTOR-sectorIndex);
			bytesLeft-=(BYTES_PER_SECTOR-sectorIndex);
			sectorIndex = 0;

			//todo: get the next cluster, if eof, create a new one

		}else{
			// just write to this cluster 
			memcpy(&OFTable[fileDescriptor].buffer[sectorIndex],buffer,nBytes);
			buffer+=nBytes;
			break;
		}
		//todo: write out to cluster fmswritesector
	}
	// set dirty
	OFTable[fileDescriptor].flags &= 0x80;

	// the file index always increases
	OFTable[fileDescriptor].fileIndex+=nBytes;
	if(OFTable[fileDescriptor].fileIndex > OFTable[fileDescriptor].fileSize){
		OFTable[fileDescriptor].fileSize = OFTable[fileDescriptor].fileIndex;
	}
	//must update filesize also
	return nBytes;
} // end fmsWriteFile
