// os345mmu.c - LC-3 Memory Management Unit
// **************************************************************************
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include "os345.h"
#include "os345lc3.h"

// ***********************************************************************
// mmu variables

// LC-3 memory
unsigned short int memory[LC3_MAX_MEMORY];

// statistics
int memAccess;						// memory accesses
int memHits;						// memory hits
int memPageFaults;					// memory faults
int nextPage;						// swap page size
int pageReads;						// page reads
int pageWrites;						// page writes

extern int lastRpte;
extern int lastUpte;
extern int uptOffset;
extern bool thisFrame;

extern bool firstTime;

int getFrame(int);
int getAvailableFrame(void);


int getFrame(int notme)
{
	int frame;
	frame = getAvailableFrame();
	if (frame >=0) return frame;

	// run clock

	int i, j, upta, frameToRemove;
	bool definedFrames = 0;
	i = lastRpte;
	j = lastUpte;
	while(1){
		// for(i=0x2400;i<0x3000;i+=2){
			// printf("%x", i);
			if(DEFINED(memory[i])){
				definedFrames = 0;
				upta = FRAME(memory[i]);
				upta = (upta<<6);

				// for(j=upta;j<upta+64;j+=2){
				while(uptOffset < 64){
					j = upta + uptOffset;
				
					if(DEFINED(memory[j])){
						definedFrames = 1;
						if(REFERENCED(memory[j])){
							memory[j] = CLEAR_REF(memory[j]);
						}else{
							//use this data frame
							frameToRemove = FRAME(memory[j]);
							lastRpte = i;
							memory[j] = CLEAR_DEFINED(memory[j]);
							memory[j] = CLEAR_REF(memory[j]);
							// memory[j] = SWAPPAGE(memory[j]);
							if(PAGED(memory[j+1])){
								memory[j+1] = accessPage(SWAPPAGE(memory[j+1]), frameToRemove, PAGE_OLD_WRITE);
							}else{
								memory[j+1] = accessPage(0, frameToRemove, PAGE_NEW_WRITE);
							}
							memory[j+1] = SET_PAGED(memory[j+1]);
							uptOffset+=2; // may not need this line
							//need to loop through rest next time but if nothing dont remove this one
							thisFrame = 1; // this upt should not be removed
							return frameToRemove;
						}
					}

					uptOffset+=2;
				}
				uptOffset = 0;

				if(!definedFrames && !thisFrame){
					frameToRemove = FRAME(memory[i]);
					lastRpte=i+2;
					memory[i] = CLEAR_DEFINED(memory[i]);
					memory[i] = CLEAR_REF(memory[i]);

					if(PAGED(memory[i+1])){
						memory[i+1] = accessPage(SWAPPAGE(memory[i+1]), frameToRemove, PAGE_OLD_WRITE);
					}else{
						memory[i+1] = accessPage(0, frameToRemove, PAGE_NEW_WRITE);
					}
					memory[i+1] = SET_PAGED(memory[i+1]);
					return frameToRemove;
					// i can swap out this upt
				}
				thisFrame = 0;

				// }
			}
		i+=2;
		if(i==0x3000){
			i=0x2400;
		}
		// }
	}

	// memory[(frameToRemove<<6)] 
	return frame;
}
// **************************************************************************
// **************************************************************************
// LC3 Memory Management Unit
// Virtual Memory Process
// **************************************************************************
//           ___________________________________Frame defined
//          / __________________________________Dirty frame
//         / / _________________________________Referenced frame
//        / / / ________________________________Pinned in memory
//       / / / /     ___________________________
//      / / / /     /                 __________frame # (0-1023) (2^10)
//     / / / /     /                 / _________page defined
//    / / / /     /                 / /       __page # (0-4096) (2^12)
//   / / / /     /                 / /       /
//  / / / /     / 	             / /       /
// F D R P - - f f|f f f f f f f f|S - - - p p p p|p p p p p p p p
// 1 word / 4 bytes / 32 bits 
// normal int is only 2 bytes
//  there is 2^16 words max memory, 2^10 frames,, each frame is 64 words = 256 bytes = 128 words
//  frame size is 2^6 words = 128 bytes = 64

#define MMU_ENABLE	1

unsigned short int *getMemAdr(int va, int rwFlg)
{
	unsigned short int pa;
	int rpta, rpte1, rpte2;
	int upta, upte1, upte2;
	int rptFrame, uptFrame;

	rpta = 0x2400 + RPTI(va); // address of the root page table
	rpte1 = memory[rpta];		// get a word
	rpte2 = memory[rpta+1];		// get another word

	// turn off virtual addressing for system RAM
	if (va < 0x3000) return &memory[va];
#if MMU_ENABLE

	// printf("using memory Management\n");

	if (DEFINED(rpte1))
	{
		// defined
	}
	else
	{
		// fault
		rptFrame = getFrame(-1);
		rpte1 = SET_DEFINED(rptFrame); // setting the frame bit in the user page table
		if (PAGED(rpte2)) // it exists in swap space
		{
			accessPage(SWAPPAGE(rpte2), rptFrame, PAGE_READ);
		}
		else
		{
			memset(&memory[(rptFrame<<6)], 0, 128); //sets 128 bytes to 0 why is it 128? why bitshift by 6?
		}
	}


	memory[rpta] = rpte1 = SET_REF(rpte1); // sets the referenced bit in the root page table
	memory[rpta+1] = rpte2;

	upta = (FRAME(rpte1)<<6) + UPTI(va);
	upte1 = memory[upta];
	upte2 = memory[upta+1];

	if (DEFINED(upte1))
	{
		// defined
	}
	else
	{

		// fault
		uptFrame = getFrame(FRAME(memory[rpta]));
		upte1 = SET_DEFINED(uptFrame);
		if (PAGED(upte2))
		{
			accessPage(SWAPPAGE(upte2), uptFrame, PAGE_READ);
		}
		else
		{
			memset(&memory[(uptFrame<<6)], 0, 128); //sets 128 bytes to 0 why is it 128? why bitshift by 6?
		}
	}

	memory[upta] = SET_REF(upte1);
	memory[upta+1] = upte2;

	return &memory[(FRAME(upte1)<<6) + FRAMEOFFSET(va)];
#else
	return &memory[va];
#endif
} // end getMemAdr


// **************************************************************************
// **************************************************************************
// set frames available from sf to ef
//    flg = 0 -> clear all others
//        = 1 -> just add bits
//
void setFrameTableBits(int flg, int sf, int ef)
{	int i, data;
	int adr = LC3_FBT-1;             // index to frame bit table
	int fmask = 0x0001;              // bit mask

	// 1024 frames in LC-3 memory
	for (i=0; i<LC3_FRAMES; i++)
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;
			adr++;
			data = (flg)?MEMWORD(adr):0;
		}
		else fmask = fmask >> 1;
		// allocate frame if in range
		if ( (i >= sf) && (i < ef)) data = data | fmask;
		MEMWORD(adr) = data;
	}
	return;
} // end setFrameTableBits


// **************************************************************************
// get frame from frame bit table (else return -1)
int getAvailableFrame()
{
	int i, data;
	int adr = LC3_FBT - 1;				// index to frame bit table
	int fmask = 0x0001;					// bit mask

	for (i=0; i<LC3_FRAMES; i++)		// look thru all frames
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;				// move to next work
			adr++;
			data = MEMWORD(adr);
		}
		else fmask = fmask >> 1;		// next frame
		// deallocate frame and return frame #
		if (data & fmask)
		{  MEMWORD(adr) = data & ~fmask;
			return i;
		}
	}
	return -1;
} // end getAvailableFrame



// **************************************************************************
// read/write to swap space
int accessPage(int pnum, int frame, int rwnFlg)
{
   static unsigned short int swapMemory[LC3_MAX_SWAP_MEMORY];

   if ((nextPage >= LC3_MAX_PAGE) || (pnum >= LC3_MAX_PAGE))
   {
   	  printf("nextPage %d pageNum %d", nextPage, pnum);	
      printf("\nVirtual Memory Space Exceeded!  (%d)", LC3_MAX_PAGE);
      exit(-4);
   }
   switch(rwnFlg)
   {
      case PAGE_INIT:                    		// init paging
         nextPage = 0;
         return 0;

      case PAGE_GET_ADR:                    	// return page address
         return (int)(&swapMemory[pnum<<6]);

      case PAGE_NEW_WRITE:                   // new write (Drops thru to write old)
         pnum = nextPage++;
   	  	// printf("nextPage %d pageNum %d", nextPage, pnum);	 

      case PAGE_OLD_WRITE:                   // write
         //printf("\n    (%d) Write frame %d (memory[%04x]) to page %d", p.PID, frame, frame<<6, pnum);
         memcpy(&swapMemory[pnum<<6], &memory[frame<<6], 1<<7);
         pageWrites++;
         return pnum;

      case PAGE_READ:                    // read
         //printf("\n    (%d) Read page %d into frame %d (memory[%04x])", p.PID, pnum, frame, frame<<6);
      	memcpy(&memory[frame<<6], &swapMemory[pnum<<6], 1<<7);
         pageReads++;
         return pnum;

      case PAGE_FREE:                   // free page
         break;
   }
   return pnum;
} // end accessPage
