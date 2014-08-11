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


void updateDirEntry(FDEntry fdEntry, DirEntry* dirEntry){
	// dirEntry->attributes = fdEntry.attributes;
	// dirEntry->startCluster = fdEntry.startCluster;
	dirEntry->fileSize = fdEntry.fileSize;
}

// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
//	Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor)
{
	printf("\n%s", "close file");
	int index = 0, error;
	DirEntry dirEntry;
	char mask[20];
	strcpy(mask, "*.*");
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

	// might need to clear out the whole buffer, we shall see
	OFTable[fileDescriptor].buffer[0] = 0;

	if(OFTable[fileDescriptor].flags & 0x80){
		error = fmsGetNextDirEntry(&index, OFTable[fileDescriptor].name, &dirEntry,CDIR);
		if(error) {
			printf("\n%s!!!!", OFTable[fileDescriptor].name);
			return error;
		}
		index--;
		char buffer[BYTES_PER_SECTOR];
		int dirIndex = index % ENTRIES_PER_SECTOR;
		int dirSector, dirCluster = CDIR;
		int loop = index/ENTRIES_PER_SECTOR;

		if(CDIR){
			while(loop--){
				dirCluster = getFatEntry(dirCluster, FAT1);
			}
			dirSector = C_2_S(dirCluster);
		}else{
			dirSector = (index/ ENTRIES_PER_SECTOR) + BEG_ROOT_SECTOR;
			if (dirSector >= BEG_DATA_SECTOR) return ERR67;
		}

		if (error = fmsReadSector(buffer, dirSector)) return error;

		fmsGetDirEntry(OFTable[fileDescriptor].name, &dirEntry);
		updateDirEntry(OFTable[fileDescriptor], &dirEntry);

		memcpy(&buffer[dirIndex * sizeof(DirEntry)], &dirEntry, sizeof(DirEntry));
		fmsWriteSector(buffer, dirSector);

		//change date and time
	}
	openFileCount--;
	OFTable[fileDescriptor].name[0] = 0;

	return 0;
} // end fmsCloseFile

void clearDirName(DirEntry* entry){
	int i;
	for(i=0;i<8;i++){
		entry->name[i] = ' ' ;
	}
	for(i=0;i<3;i++){
		entry->extension[i] = ' ' ;
	}
}

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
	printf("\n%s %s %x", "define file", fileName, attribute);
	char mask[20];
	strcpy(mask, "*.*");
	DirEntry dirEntry, createdDir;
	int i, error, nameindex = 0, index =0;
	if(isValidFileName(fileName)!=1) return ERR50;
	error = fmsGetNextDirEntry(&index, fileName, &dirEntry, CDIR);
	if(!error) return ERR60;

	clearDirName(&createdDir);

	//find the spot to put it in
	index = 0;
	while(1)
	{
		error = fmsGetNextDirEntry(&index, mask, &dirEntry, CDIR);
		if(error)
		{
			if (error != ERR67) fmsError(error);
			break;
		}
	}
	// index--; this should not be used
	// index should be the place to put it in now, might have to change by 1?
	char buffer[BYTES_PER_SECTOR];
	int dirIndex = index % ENTRIES_PER_SECTOR;
	int dirSector, dirCluster = CDIR;
	int loop = (index-1)/ENTRIES_PER_SECTOR;

	if(CDIR){
		while(loop--){
			dirCluster = getFatEntry(dirCluster, FAT1);
		}
		dirSector = C_2_S(dirCluster);
	}else{
		dirSector = (index/ ENTRIES_PER_SECTOR) + BEG_ROOT_SECTOR;
		if (dirSector >= BEG_DATA_SECTOR) return ERR67;
	}

	if (error = fmsReadSector(buffer, dirSector)) return error;

	//fill up information about directory
	bool ext = 0;
	for(i=0;i<12;i++){
		if(fileName[i]==0){
			break;
		}
		if(fileName[i]=='.'){
			nameindex = 0;
			ext = 1;
			continue;
		}
		if(!ext){
			createdDir.name[nameindex++] = toupper(fileName[i]);
		}else{
			createdDir.extension[nameindex++] = toupper(fileName[i]);
		}
	}
	createdDir.attributes = attribute ? attribute : 0x20;
	createdDir.fileSize = 0;
	//create the time and date here
	
	if(attribute==DIRECTORY){
		int fatindex = nextFATindex(FAT1);
		if(fatindex<0){
			return fatindex;
		}
		// i am not sure if this fatindex is correct..	
		createdDir.startCluster = fatindex;

		printf("%d\n", C_2_S(fatindex));

		setFatEntry(fatindex, FAT_EOC, FAT1);
		setFatEntry(fatindex, FAT_EOC, FAT2);	
		//create the . and ..


		DirEntry dotEntry;
		DirEntry dotDotEntry;
		clearDirName(&dotEntry);
		clearDirName(&dotDotEntry);
		char newBuffer[BYTES_PER_SECTOR];
		dotEntry.name[0] = '.';
		for(i=0;i<2;i++) dotDotEntry.name[i] = '.';
		dotEntry.attributes = attribute;
		dotDotEntry.attributes = attribute;
		dotEntry.fileSize = 0;
		dotDotEntry.fileSize = 0;
		dotEntry.startCluster = fatindex;
		dotDotEntry.startCluster = CDIR;

		if (error = fmsReadSector(newBuffer,C_2_S(fatindex))) return error;
		memcpy(&newBuffer[0 * sizeof(DirEntry)], &dotEntry, sizeof(DirEntry));
		memcpy(&newBuffer[1 * sizeof(DirEntry)], &dotDotEntry, sizeof(DirEntry));
		fmsWriteSector(newBuffer, C_2_S(fatindex));

	}else{
		int fatindex = nextFATindex(FAT1);
		setFatEntry(fatindex, FAT_EOC, FAT1);
		setFatEntry(fatindex, FAT_EOC, FAT2);

		createdDir.startCluster = fatindex;
	}
	memcpy(&buffer[dirIndex * sizeof(DirEntry)], &createdDir, sizeof(DirEntry));
	fmsWriteSector(buffer, dirSector);
	return 0;
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
	printf("\ndeleting a file");
	char name[13];
	char mask[20];
	strcpy(mask, "*.*");
	DirEntry dirEntry;
	int i, error, nameCount = 0, index =0;

	if(isValidFileName(fileName)!=1) return ERR50;
	error = fmsGetNextDirEntry(&index, fileName, &dirEntry, CDIR);
	if (error) return (error ? ((error == ERR67) ? ERR61 : error) : 0);

	index--;

	char buffer[BYTES_PER_SECTOR];
	int dirSector, dirIndex, dirCluster = CDIR;
	int loop = index/ENTRIES_PER_SECTOR;

	// get the right sector 
	if(CDIR){
		while(loop--){
			dirCluster = getFatEntry(dirCluster, FAT1);
		}
		dirSector = C_2_S(dirCluster);
	}else{
		dirSector = (index/ ENTRIES_PER_SECTOR) + BEG_ROOT_SECTOR;
		if (dirSector >= BEG_DATA_SECTOR) return ERR67;
	}

	if (error = fmsReadSector(buffer, dirSector)) return error;

	dirIndex = index % ENTRIES_PER_SECTOR;
	memcpy(&dirEntry, &buffer[dirIndex * sizeof(DirEntry)], sizeof(DirEntry));

	if(dirEntry.attributes & 0x10){
		//directory, check if empty err69
		DirEntry emptyDirEnty;
		int delIndex = 0;
		//there should be at least 2 entries = . and ..
		for(i=0;i<3;i++){
			error = fmsGetNextDirEntry(&delIndex, mask, &emptyDirEnty, dirEntry.startCluster);
		}
		// if(!error){
		// 	//there is something in the dir, cannot del
		// 	return ERR69;
		// }
	}

	dirEntry.name[0] = 0xe5;
	memcpy(&buffer[dirIndex * sizeof(DirEntry)], &dirEntry, sizeof(DirEntry));
	fmsWriteSector(buffer, dirSector);

	//clearing cluster chain
	int curCluster = dirEntry.startCluster;
	int nextCluster = 0;
	// while(getFatEntry(curCluster, FAT1)!= FAT_EOC)
	// {
	// 	nextCluster = getFatEntry(curCluster, FAT1);
	// 	setFatEntry(curCluster, 0, FAT1);
	// 	curCluster = nextCluster;
	// }
	setFatEntry(curCluster, 0, FAT1);
	return 0;
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
}

int fmsOpenFile(char* fileName, int rwMode)
{
	printf("\n%s %s in %d mode", "open file", fileName, rwMode);

	int error = 0;
	int index = 0;
	int i,j;
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
			// printf("\n%s %s in %d mode", "open file", fileName, rwMode);
			if (error == ERR67) return ERR61;
			break;
		}

		//check if file or dir
		if(!(dirEntry.attributes & 0x10) && !(dirEntry.attributes & 0x08) ){
			bool sameName = 1;
			for(i=0;i<12;i++){
				if(name[i]!=dirEntry.name[i]){
					sameName = 0;
				}
			}
			if(sameName){
				if(openFileCount>=32){
					return ERR70;
				}
				// check if file is already open
				for(i=0;i<NFILES;i++){
					sameName = 1;
					for(j=0;j<12;j++){
						if(name[j]!=OFTable[i].name[j]){
							sameName = 0;
						}
					}
					if(sameName){
						return ERR62;
					}
				}
				//create FDEntry
				FDEntry fdEntry;
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
				if(rwMode!=0){
					OFTable[fileDescriptor].flags |= 0x80;
				}
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
	// printf("\n%s, %d bytes", "read file", nBytes);
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
	// printf("\nsize %d index %d", fileSize, fileIndex);
	if(fileSize==fileIndex){
		return ERR66;
	}
	int fileLeft = fileSize-fileIndex;
	if(nBytes > fileLeft){
		nBytes = fileLeft;
	}

	int sectorIndex = fileIndex % BYTES_PER_SECTOR;
	int bytesLeft = nBytes;
	int indexIntoBuffer = 0;
	while(bytesLeft){
		if ((error = fmsReadSector(OFTable[fileDescriptor].buffer,C_2_S(OFTable[fileDescriptor].currentCluster)))) return error;
		if(bytesLeft + sectorIndex >= BYTES_PER_SECTOR){
			//read all bytes left in this sector, go to next sector
			memcpy(buffer, &OFTable[fileDescriptor].buffer[sectorIndex], BYTES_PER_SECTOR-sectorIndex);
			buffer+=(BYTES_PER_SECTOR-sectorIndex);
			bytesLeft-=(BYTES_PER_SECTOR-sectorIndex);
			sectorIndex = 0;
			if ((nextCluster = getFatEntry(OFTable[fileDescriptor].currentCluster, FAT1)) != FAT_EOC){
			         OFTable[fileDescriptor].currentCluster = nextCluster;
			}else{
				printf("\n%s %d", "this should not happen", bytesLeft);
				return ERR66;
			}
		}else{
			memcpy(buffer, &OFTable[fileDescriptor].buffer[sectorIndex], bytesLeft);

			// char buffer2[BYTES_PER_SECTOR];
			// memcpy(buffer2, &OFTable[fileDescriptor].buffer[sectorIndex], bytesLeft);
			// printf("\n%s", buffer2);
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
	printf("\n%s index %d", "seek file", index);

	if(OFTable[fileDescriptor].name[0]==0){
		return ERR63;
	}
	if(OFTable[fileDescriptor].mode==1 || OFTable[fileDescriptor].mode==2){
		return ERR84;
	}
	if(OFTable[fileDescriptor].fileSize < index){
		return ERR80;
	}

	int clusterNum = OFTable[fileDescriptor].startCluster;
	int start = 0;
	while(index > 0){
		if(index < BYTES_PER_SECTOR){
			OFTable[fileDescriptor].currentCluster = clusterNum;
			break;
		}
		index-=BYTES_PER_SECTOR;
		clusterNum = getFatEntry(clusterNum,FAT1);
	}

	OFTable[fileDescriptor].fileIndex = index;
	return index;
} // end fmsSeekFile


int nextFATindex(unsigned char* FAT){
	int i, entries;
	unsigned short fatvalue;
	entries = (512 * 9) / 1.5;
	for(i=2;i<entries;i++){
		fatvalue = getFatEntry(i,FAT);
		if(!fatvalue){
			return i;
		}
	}
	return ERR65;
}

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
	// printf("\n%s buffer (%s) of lengt1h %d", "write file", buffer, nBytes);

	if(OFTable[fileDescriptor].name[0]==0){
		return ERR63;
	}
	if(OFTable[fileDescriptor].mode==0){
		return ERR84;
	}
	int error, fatindex;
	short nextCluster;


	int fileIndex = OFTable[fileDescriptor].fileIndex;
	int sectorIndex = fileIndex % BYTES_PER_SECTOR;
	int bytesLeft = nBytes;



	//init the currentcluster
	if(!OFTable[fileDescriptor].startCluster){
		OFTable[fileDescriptor].startCluster = nextFATindex(FAT1);
		OFTable[fileDescriptor].currentCluster = OFTable[fileDescriptor].startCluster;
		setFatEntry(OFTable[fileDescriptor].currentCluster, (unsigned short)FAT_EOC, FAT1);
		setFatEntry(OFTable[fileDescriptor].currentCluster, (unsigned short)FAT_EOC, FAT2);
	}

	while(bytesLeft){
		//read into buffer for writing out later
		if ((error = fmsReadSector(OFTable[fileDescriptor].buffer,C_2_S(OFTable[fileDescriptor].currentCluster)))) return error;

		if(nBytes >= BYTES_PER_SECTOR - sectorIndex){
			// i need to write to the next cluster, write what i can first.. 	
			printf("\nthe starting cluster is %d", OFTable[fileDescriptor].startCluster);
			memcpy(&OFTable[fileDescriptor].buffer[sectorIndex],buffer,BYTES_PER_SECTOR-sectorIndex);
			buffer+=(BYTES_PER_SECTOR-sectorIndex);
			bytesLeft-=(BYTES_PER_SECTOR-sectorIndex);
			sectorIndex = 0;

			//write out to cluster fmswritesector
			if ((error = fmsWriteSector(OFTable[fileDescriptor].buffer,C_2_S(OFTable[fileDescriptor].currentCluster)))) return error;

			//get the next cluster, if eof, create a new one
			if((nextCluster = getFatEntry(OFTable[fileDescriptor].currentCluster, FAT1)) 
				!= FAT_EOC){
				OFTable[fileDescriptor].currentCluster = nextCluster;
			}else{
				//get a new cluster
				fatindex = nextFATindex(FAT1);
				if(fatindex<0){
					return fatindex;
				}
				setFatEntry(OFTable[fileDescriptor].currentCluster,fatindex, FAT1);
				setFatEntry(OFTable[fileDescriptor].currentCluster, fatindex, FAT2);
				OFTable[fileDescriptor].currentCluster = fatindex;
				// int newCluster = OFTable[fileDescriptor].currentCluster;
				setFatEntry(fatindex, FAT_EOC, FAT1);
				setFatEntry(fatindex, FAT_EOC, FAT2);
			}
		}else{
			// just write to this cluster 
			memcpy(&OFTable[fileDescriptor].buffer[sectorIndex],buffer,nBytes);
			buffer+=nBytes;
			if ((error = fmsWriteSector(OFTable[fileDescriptor].buffer,C_2_S(OFTable[fileDescriptor].currentCluster)))) return error;
			break;
			//write out to cluster fmswritesector
		}
	}
	// set dirty
	OFTable[fileDescriptor].flags |= 0x80;

	// the file index always increases
	OFTable[fileDescriptor].fileIndex+=nBytes;
	if(OFTable[fileDescriptor].fileIndex > OFTable[fileDescriptor].fileSize){
		OFTable[fileDescriptor].fileSize = OFTable[fileDescriptor].fileIndex;
	}
	//must update filesize also
	return nBytes;
} // end fmsWriteFile
