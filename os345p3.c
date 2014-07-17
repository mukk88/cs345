// os345p3.c - Jurassic Park
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>
#include "os345.h"
#include "os345park.h"

// ***********************************************************************
// project 3 variables

// Jurassic Park
extern JPARK myPark;
extern Semaphore* parkMutex;						// protect park access
extern Semaphore* fillSeat[NUM_CARS];			// (signal) seat ready to fill
extern Semaphore* seatFilled[NUM_CARS];		// (wait) passenger seated
extern Semaphore* rideOver[NUM_CARS];			// (signal) ride over

extern Semaphore* deltaClockSem;

// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);

typedef struct
{
	int time;
	Semaphore* sem;
} DeltaClock;

DeltaClock* deltaClock;
int deltaClockSize;
int deltaClockCount;

void initDeltaClock()
{
	deltaClock = malloc(sizeof(DeltaClock) * 100);
	deltaClockSize = 0;
}

void swapDeltaClock(int index)
{
	DeltaClock temp;
	temp.time = deltaClock[index].time;
	temp.sem = deltaClock[index].sem;

	deltaClock[index].time = deltaClock[index-1].time;
	deltaClock[index].sem = deltaClock[index-1].sem;

	deltaClock[index-1].time = temp.time;
	deltaClock[index-1].sem = temp.sem;
}

void adjustDeltaClock()
{
	int i;
	for(i=deltaClockSize;i>0;i--)
	{
		if(deltaClock[i].time > deltaClock[i-1].time)
		{
			deltaClock[i].time -= deltaClock[i-1].time;
			swapDeltaClock(i);

		}else
		{
			deltaClock[i-1].time -= deltaClock[i].time;
			break;
		}
	}
}

void addDeltaClock(int time, Semaphore* sem)
{
	int i;
	deltaClock[deltaClockSize].sem = sem;
	deltaClock[deltaClockSize].time = time;
	adjustDeltaClock();
	deltaClockSize++;
}

// display all pending events in the delta clock list
void printDeltaClock(void)
{
	int i;
	// printf("\n%s", "printing delta clock");
	for (i=deltaClockSize-1; i>=0; i--)
	{
		printf("\n%4d%4d  %-20s", i, deltaClock[i].time, deltaClock[i].sem->name);
	}
	return;
}

extern Semaphore* tics10thsec;

// ********************************************************************************************
// display time every tics1sec
int timeTask(int argc, char* argv[])
{
	int tempDeltaClockSize=0;
	char svtime[64];						// ascii current time
	while (1)
	{
		SEM_WAIT(tics10thsec)
		SEM_WAIT(deltaClockSem)
		if(deltaClockSize>0)
		{
			deltaClock[deltaClockSize-1].time--;
			while(deltaClock[deltaClockSize-1].time<=0)
			{
				//signal that sem
				deltaClockSize--;
			}
		}
		if(tempDeltaClockSize!=deltaClockSize)
		{
			printDeltaClock();
		}
		tempDeltaClockSize = deltaClockSize;
		// printf("\nTime = %s", myTime(svtime));
		SEM_SIGNAL(deltaClockSem)
	}
	return 0;
} // end timeTask

// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[])
{
	// deltaClockCount = 0;
	// initDeltaClock();
	// static char* deltaArgv[] = {"deltaClockTask", "2"};

	// Semaphore* testMutex = createSemaphore("testMutex", BINARY, 1);
	// addDeltaClock(10, testMutex);
	// addDeltaClock(100, testMutex);
	// addDeltaClock(100, testMutex);
	// addDeltaClock(120, testMutex);

	// createTask("DeltaClockTask",
	// 	timeTask,
	// 	HIGH_PRIORITY,
	// 	2,
	// 	deltaArgv);
	int i;
	char buf[32];
	char buf2[32];
	char* newArgv[2];

	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask( buf,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument


	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");

	for(i=0;i<NUM_DRIVERS;i++)
	{	
		sprintf(buf2, "%d", i);
		sprintf(buf, "driver%d", i);

		newArgv[0] = buf2;


		createTask(buf,
			driverTask,
			MED_PRIORITY,
			1,
			newArgv
		);
	}


	for(i=0;i<12;i++)
	{	
		sprintf(buf2, "%d", i);
		sprintf(buf, "visitor%d", i);

		newArgv[0] = buf2;


		createTask(buf,
			visitorTask,
			MED_PRIORITY,
			1,
			newArgv
		);
	}
	// ?? create car, driver, and visitor tasks here

	return 0;
} // end project3



// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	// must do p3 first
	char buf[50];
	printf("\nAdding Delta Clock");
	sprintf(buf, "testMutex%d", deltaClockCount++);
	Semaphore* testMutex = createSemaphore(buf, BINARY, 1);
	SEM_WAIT(deltaClockSem)
	addDeltaClock(atoi(argv[1]), testMutex);
	// printDeltaClock();
	SEM_SIGNAL(deltaClockSem)
	return 0;
} // end CL3_dc

// ***********************************************************************
// test delta clock
// int P3_tdc(int argc, char* argv[])
// {
// 	createTask( "DC Test",			// task name
// 		dcMonitorTask,		// task
// 		10,					// task priority
// 		argc,					// task arguments
// 		argv);

// 	timeTaskID = createTask( "Time",		// task name
// 		timeTask,	// task
// 		10,			// task priority
// 		argc,			// task arguments
// 		argv);
// 	return 0;
// } // end P3_tdc


// ***********************************************************************
// monitor the delta clock task
// int dcMonitorTask(int argc, char* argv[])
// {
// 	int i, flg;
// 	char buf[32];
// 	// create some test times for event[0-9]
// 	int ttime[10] = {
// 		90, 300, 50, 170, 340, 300, 50, 300, 40, 110	};

// 	Semaphore* event[10];

// 	for (i=0; i<10; i++)
// 	{
// 		sprintf(buf, "event[%d]", i);
// 		event[i] = createSemaphore(buf, BINARY, 0);
// 		addDeltaClock(ttime[i], event[i]);
// 	}
// 	printDeltaClock();

// 	while (deltaClockSize > 0)
// 	{
// 		SEM_WAIT(dcChange)
// 		flg = 0;
// 		for (i=0; i<10; i++)
// 		{	
// 			if (event[i]->state ==1)			{
// 					printf("\n  event[%d] signaled", i);
// 					event[i]->state = 0;
// 					flg = 1;
// 				}
// 		}
// 		if (flg) printDeltaClock();
// 	}
// 	printf("\nNo more events in Delta Clock");

// 	// // kill dcMonitorTask
// 	tcb[timeTaskID].state = S_EXIT;
// 	return 0;
// } // end dcMonitorTask







