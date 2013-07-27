/* sh6bench.c -- SmartHeap (tm) Portable memory management benchmark.
 *
 * Copyright (C) 2000 MicroQuill Software Publishing Corporation.
 * All Rights Reserved.
 *
 * No part of this source code may be copied, modified or reproduced
 * in any form without retaining the above copyright notice.
 * This source code, or source code derived from it, may not be redistributed
 * without express written permission of the copyright owner.
 *
 *
 * Compile-time flags.  Define the following flags on the compiler command-line
 * to include the selected APIs in the benchmark.  When testing an ANSI C
 * compiler, include MALLOC_ONLY flag to avoid any SmartHeap API calls.
 * Define these symbols with the macro definition syntax for your compiler,
 * e.g. -DMALLOC_ONLY=1 or -d MALLOC_ONLY=1
 *
 *  Flag                   Meaning
 *  -----------------------------------------------------------------------
 *  MALLOC_ONLY=1       Test ANSI malloc/realloc/free only
 *  INCLUDE_NEW=1       Test C++ new/delete
 *  INCLUDE_MOVEABLE=1  Test SmartHeap handle-based allocation API
 *  MIXED_ONLY=1        Test interdispersed alloc/realloc/free only
 *                      (no tests for alloc, realloc, free individually)
 *  SYS_MULTI_THREAD=1  Test with multiple threads (OS/2, NT, HP, Solaris only)
 *  SMARTHEAP=1         Required when compiling if linking with SmartHeap lib
 * 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include "atomic.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_Windows) || defined(_WINDOWS) || defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#include <windows.h>
#define WIN32 1
#if defined(SYS_MULTI_THREAD)
#include <process.h>
typedef HANDLE ThreadID;
#define THREAD_NULL (ThreadID)-1
#endif /* SYS_MULTI_THREAD */

#elif defined(UNIX) || defined(__hpux) || defined(_AIX) || defined(__sun) \
		|| defined(sgi) || defined(__DGUX__) || defined(__linux__)
/* Unix prototypes */
#ifndef UNIX
#define UNIX 1
#endif
#include <unistd.h>
#ifdef SYS_MULTI_THREAD
#if defined(__hpux) || defined(__osf__) || defined(_AIX) || defined(sgi) \
		|| defined(__DGUX__) || defined(__linux__)
#define _INCLUDE_POSIX_SOURCE
#include <sys/signal.h>
#include <pthread.h>
typedef pthread_t ThreadID;
#if defined(_DECTHREADS_) && !defined(__osf__)
ThreadID ThreadNULL = {0, 0, 0};
#define THREAD_NULL ThreadNULL
#define THREAD_EQ(a,b) pthread_equal(a,b)
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif /* __hpux */
int thread_specific;
#elif defined(__sun)
#include <thread.h>
typedef thread_t ThreadID;
#endif /* __sun */
#endif /* SYS_MULTI_THREAD */

#elif defined(__OS2__) || defined(__IBMC__)
#ifdef __cplusplus
extern "C" {
#endif
#include <process.h>
#define INCL_DOS
#define INCL_DOSPROCESS    /* thread control	*/
#define INCL_DOSMEMMGR     /* Memory Manager values */
#define INCL_DOSERRORS     /* Get memory error codes */
#include <os2.h>
#include <bsememf.h>      /* Get flags for memory management */
typedef TID ThreadID; 
#define THREAD_NULL (ThreadID)-1
#ifdef __cplusplus
}
#endif

#endif /* end of environment-specific header files */

#ifndef THREAD_NULL
#define THREAD_NULL 0
#endif
#ifndef THREAD_EQ
#define THREAD_EQ(a,b) ((a)==(b))
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef PRINTLATENCY
	#define COLLECTLATENCY latency = rdtsc()
	#define SHOWLATENCY(msg, thread_id)  fprintf(fout, "latency %s %d %ld %ld\n", msg, thread_id, latency, rdtsc()-latency)
#else
	#define COLLECTLATENCY 0
	#define SHOWLATENCY(msg, thread_id) 0
#endif

/* Note: the SmartHeap header files must be included _after_ any files that
 * declare malloc et al
 */
//#include "smrtheap.h"
#ifdef MALLOC_ONLY
#undef malloc
#undef realloc
#undef free
#endif

/* To test STM we include libscm.h */
#ifdef STR_MALLOC
#include "libscm.h"
#endif
#ifdef STM_MALLOC
#include "libscm.h"
#endif
#ifdef STRMC_MALLOC
#include "libscm.h"
#endif
/*******************************/

#include <malloc.h>
#ifdef PRINTMEMINFO
       #include <malloc.h>
#ifndef SYS_MULTI_THREAD
       #include <pthread.h>
#endif
       static int netmem = 0;
       static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
       #define UPDATENETMEM(netmem_delta) pthread_mutex_lock(&mutex);\
                       netmem += netmem_delta; \
                       pthread_mutex_unlock(&mutex)
       #define SHOWMEMINFO pthread_mutex_lock(&mutex);\
                       fprintf(stderr, "Thread/Netmem: %lu %d\n", pthread_self(), netmem);\
                       malloc_stats();\
                       pthread_mutex_unlock(&mutex)
#else
       #define UPDATENETMEM(netmem_delta) 0
       #define SHOWMEMINFO 0
#endif

#ifdef INCLUDE_NEW
#include "smrtheap.hpp"
#endif

#if defined(SHANSI)
#include "shmalloc.h"
int SmartHeap_malloc = 0;
#endif

#ifdef SILENT
void fprintf_silent(FILE *, ...);
void fprintf_silent(FILE *x, ...) { (void)x; }
#else
#define fprintf_silent fprintf
#endif

#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif

#ifdef CLK_TCK
#undef CLK_TCK
#endif
#define CLK_TCK CLOCKS_PER_SEC
#ifndef CALL_COUNT
#define CALL_COUNT 1000
#endif

#ifndef CPUS
#define CPUS 4
#endif

#define TRUE 1
#define FALSE 0
typedef int Bool;

FILE *fout, *fin;
unsigned uMaxBlockSize = 1000;
unsigned uMinBlockSize = 1;
unsigned long ulCallCount = CALL_COUNT;
unsigned long uCPUs = CPUS;

unsigned long promptAndRead(char *msg, unsigned long defaultVal, char fmtCh);

#ifdef SYS_MULTI_THREAD
unsigned uThreadCount = 8;
ThreadID RunThread(void (*fn)(void *), void *arg);
void WaitForThreads(ThreadID[], unsigned);
int GetNumProcessors(void);
#else
unsigned uThreadCount = 1;
#endif

#ifdef HEAPALLOC_WRAPPER
#define TEST_HEAPALLOC 1
#endif

#ifdef TEST_HEAPALLOC
#ifdef malloc
#undef malloc
#undef realloc
#undef free
#endif

#define malloc(s) HeapAlloc(GetProcessHeap(), 0, s)
#define realloc(p, s) HeapReAlloc(GetProcessHeap(), 0, p, s)
#define free(p) HeapFree(GetProcessHeap(), 0, p)

#endif

void doBench(void *);

int getPagefault() {
	int pageFault=0;
	char line[500];
	char filename[50];	
	sprintf(filename, "/proc/%d/stat", getpid());

	FILE* statfile = fopen(filename, "r");
	if(!statfile) {
		fprintf(stderr, "Could not open filename\n");
		exit(0);
	}
	int d_count = 0;
	char c;	
	while(d_count != 9) {
		c = fgetc(statfile);
		if(c == ' ') {
			d_count++;		
		}
	}
	char c_minor_pagefault[10];
	int p = 0;
	while(c=fgetc(statfile)) {
		if(c != ' ') {
			c_minor_pagefault[p]=c;
		} else {
			c_minor_pagefault[p]='\0';
			break;
		}
		p++;
	}
	c_minor_pagefault[p] = '\0';
	//printf("page fault = %s\n", c_minor_pagefault);
	pageFault = atoi(c_minor_pagefault);
	fclose(statfile);
	return pageFault;
}

void main(int argc, char *argv[])
{
	clock_t startCPU;
	time_t startTime;
	double elapsedTime, cpuTime;
	uint64_t clock_reg;

	int pf0 = getPagefault();	

#ifdef SMARTHEAP
	MemRegisterTask();
#endif

	setbuf(stdout, NULL);  /* turn off buffering for output */

	if (argc > 1)
		fin = fopen(argv[1], "r");
	else
		fin = stdin;
	if (argc > 2)
		fout = fopen(argv[2], "w");
	else
		fout = stdout;

	if(fin == NULL || fout == NULL) {
		fprintf(stderr, "Could not open file(s): ");
		int i=1;
		for(i=1; i<argc;i++) {
			fprintf(stderr, "%s ", argv[i]);
		}
		fprintf(stderr, "\n");
		exit(-1);
	}

	ulCallCount = promptAndRead("call count", ulCallCount, 'u');
	uMinBlockSize = (unsigned)promptAndRead("min block size",uMinBlockSize,'u');
	uMaxBlockSize = (unsigned)promptAndRead("max block size",uMaxBlockSize,'u');

#ifdef HEAPALLOC_WRAPPER
	LoadLibrary("shsmpsys.dll");
#endif

#ifdef SYS_MULTI_THREAD
	{
		unsigned i;
		void *threadArg = NULL;
		ThreadID *tids;

#ifdef WIN32
		//unsigned uCPUs = promptAndRead("CPUs (0 for all)", 0, 'u');

		if (uCPUs)
		{
			DWORD m1, m2;

			if (GetProcessAffinityMask(GetCurrentProcess(), &m1, &m2))
			{
				i = 0;
				m1 = 1;

				/*
				 * iterate through process affinity mask m2, counting CPUs up to
				 * the limit specified in uCPUs
				 */
				do
					if (m2 & m1)
						i++;
				while ((m1 <<= 1) && i < uCPUs);

				/* clear any extra CPUs in affinity mask */
				do
					if (m2 & m1)
						m2 &= ~m1;
				while (m1 <<= 1);

				if (SetProcessAffinityMask(GetCurrentProcess(), m2))
					fprintf(fout,
							"\nThreads in benchmark will run on max of %u CPUs", i);
			}
		}
#endif /* WIN32 */

		uThreadCount = uCPUs;//(int)promptAndRead("threads", GetNumProcessors(), 'u');

		if (uThreadCount < 1)
			uThreadCount = 1;
		ulCallCount /= uThreadCount;
		if ((tids = malloc(sizeof(ThreadID) * uThreadCount)) != NULL)
		{
			startCPU = clock();
			startTime = time(NULL);
			clock_reg = rdtsc();
                        UPDATENETMEM(mallinfo().uordblks + ulCallCount * sizeof(void *));

			for (i = 0;  i < uThreadCount;  i++) {
				if (THREAD_EQ(tids[i] = RunThread(doBench, threadArg),THREAD_NULL))
				{
					fprintf(fout, "\nfailed to start thread #%d", i);
					break;
				}
			}
			WaitForThreads(tids, uThreadCount);
			free(tids);
		}
		if (threadArg)
			free(threadArg);
	}
#else
        UPDATENETMEM(mallinfo().uordblks + ulCallCount * sizeof(void *));
	startCPU = clock();
	startTime = time(NULL);
	clock_reg = rdtsc();
	doBench(NULL);
#endif

	uint64_t cpuTime_reg = (rdtsc() - clock_reg);
	elapsedTime = difftime(time(NULL), startTime);
	cpuTime = (double)(clock()-startCPU) / (double)CLK_TCK;
//	cpuTime = (double)(clock()-startCPU);
//	uint64_t cpuTime_reg = (rdtsc() - clock_reg) / 2246796049;

	fprintf_silent(fout, "\n");
#ifdef PRINTTHROUGHPUT
	fprintf(fout, "throughput %ld", (long) (cpuTime_reg));
#endif
	fprintf(fout, "\nTotal elapsed time"
#ifdef SYS_MULTI_THREAD
			" for %d threads"
#endif
			": %.2f (%.4f CPU) (%ld clock ticks read from register)\n",
#ifdef SYS_MULTI_THREAD
			uThreadCount,
#endif
			elapsedTime, cpuTime, cpuTime_reg);

	if (fin != stdin)
		fclose(fin);
	if (fout != stdout)
		fclose(fout);
	
	printf("Occurred page faults: %d\n", getPagefault() - pf0);
}

void doBench(void *arg)
{

#ifdef PRINTTHROUGHPUT
	unsigned int num_objects = 0;
#endif
	char **memory = malloc(ulCallCount * sizeof(void *));
        size_t memory_sizes[ulCallCount * sizeof(size_t)];

#ifdef STRMC_MALLOC
	const int slowClock = scm_register_clock();
	const int fastClock = scm_register_clock();
#endif
        int mpindex = 0;
        int mpeindex = ulCallCount;
        int save_start_index = mpeindex;
        int save_end_index = mpeindex;
	int size_base, size, iterations;
	int repeat = ulCallCount;
	char **mp = memory;
	char **mpe = memory + ulCallCount;
	char **save_start = mpe;
	char **save_end = mpe;
#if STM_MALLOC || STR_MALLOC || STRMC_MALLOC
	const int old_buf_size = ulCallCount / 5;
	char **old_buf_start = memory;
#endif

	uint64_t latency;

        UPDATENETMEM(ulCallCount * sizeof(void*));
	SHOWMEMINFO;

#if STR_MALLOC || STRMC_MALLOC
	int current_index = 0;
	int surviving_regions[2];
	memset(surviving_regions, -1, 2*sizeof(int));

	COLLECTLATENCY;
	int temporary_region = scm_create_region();
	SHOWLATENCY("scm_create_region", pthread_self());

	COLLECTLATENCY;
	surviving_regions[current_index] = scm_create_region();
	SHOWLATENCY("scm_create_region", pthread_self());
#endif
#if STR_MALLOC

	COLLECTLATENCY;
	scm_refresh_region(temporary_region, 0);
	SHOWLATENCY("scm_refresh_region", pthread_self());

	COLLECTLATENCY;
	scm_refresh_region(surviving_regions[current_index], 1);
	SHOWLATENCY("scm_refresh_region", pthread_self());
#elif STRMC_MALLOC
	COLLECTLATENCY;
	scm_refresh_region_with_clock(temporary_region, 0, fastClock);
	SHOWLATENCY("scm_refresh_region_with_clock", pthread_self());

	COLLECTLATENCY;
	scm_refresh_region_with_clock(surviving_regions[current_index], 0, slowClock);
	SHOWLATENCY("scm_refresh_region_with_clock", pthread_self());
#endif


	while (repeat--)
	{
		for (size_base = uMinBlockSize;
				size_base < uMaxBlockSize;
				size_base = size_base * 3 / 2 + 1)
		{
			for (size = size_base; size; size /= 2)
			{
				/* allocate smaller blocks more often than large */
				iterations = 1;

				if (size < 10000)
					iterations = 10;

				if (size < 1000)
					iterations *= 5;

				if (size < 100)
					iterations *= 5;


				while (iterations--)
				{
#if STR_MALLOC || STRMC_MALLOC
					// if the pointers belong to the part of the memory which
					// are still needed after an iteration, we put them into
					// another region
					COLLECTLATENCY;
					if((mp >= old_buf_start) && (mp <= (old_buf_start + old_buf_size))) {

						if (!memory || !(*mp = scm_malloc_in_region(size, surviving_regions[current_index])))
						{
							fprintf(stderr, "Out of memory\n");
							_exit (1);
						}
					} else {
						if (!memory || !(*mp = scm_malloc_in_region(size, temporary_region)))
						{
							fprintf(stderr, "Out of memory\n");
							_exit (1);
						}
					}
					SHOWLATENCY("scm_malloc_in_region", pthread_self());
					memory_sizes[mpindex] = size;
					UPDATENETMEM(size);
					mp ++;
					mpindex++;
#elif STM_MALLOC
					COLLECTLATENCY;
					if (!memory || !(*mp = (char *)scm_malloc(size)))
					{
						fprintf(stderr, "Out of memory\n");
						_exit (1);
					}
					SHOWLATENCY("scm_malloc", pthread_self());

					if((mp >= old_buf_start) && (mp <= (old_buf_start + old_buf_size))) {
						COLLECTLATENCY;
						scm_refresh(*mp, 1);
						SHOWLATENCY("scm_refresh", pthread_self());
					} else {
						COLLECTLATENCY;
						scm_refresh(*mp, 0);
						SHOWLATENCY("scm_refresh", pthread_self());
					}

					memory_sizes[mpindex] = size;
					UPDATENETMEM(size);
					mp ++;
					mpindex++;
#else
					COLLECTLATENCY;
					//printf("allocating into memory address %p\n", mp);
					if (!memory || !(*mp++ = (char *)malloc(size)))
					{
						printf("Out of memory\n");
						_exit (1);
					}
#ifdef PRINTTHROUGHPUT
					else {
						num_objects++;
					}
#endif
					SHOWLATENCY("malloc", pthread_self());
					memory_sizes[mpindex] = size;
					UPDATENETMEM(size);
					mpindex++;
#endif

					/* while allocating skip over that portion of the buffer that still
	     	 	 	 holds pointers from the previous cycle
					 */
					if (mp == save_start) {
						//printf("skipping memory %p - %p\n", mp, save_end);
						mp = save_end;
						mpindex = save_end_index;
					}

					if (mp >= mpe)   /* if we've reached the end of the malloc buffer */
					{
						SHOWMEMINFO;
						mp = memory;
						mpindex = 0;
						//printf("mp: %p\n", mp);

						/* mark the next portion of the buffer */
						save_start = save_end;
						save_start_index = save_end_index;
						if (save_start >= mpe) {
							save_start = mp;
							save_start_index = mpindex;
						}

						save_end = save_start + (ulCallCount / 5);
						save_end_index = save_start_index + (ulCallCount / 5);
						if (save_end > mpe) {
							save_end = mpe;
							save_end_index = mpindex;
						}
						/* free the bottom and top parts of the buffer.
						 * The bottom part is freed in the order of allocation.
						 * The top part is free in reverse order of allocation.
						 */
#if STR_MALLOC || STM_MALLOC
						COLLECTLATENCY;
						scm_tick();
						SHOWLATENCY("scm_tick", pthread_self());
#elif STRMC_MALLOC

						// 1 - current_index is
						// 1 if current_index = 0 and
						// 0 if current_index = 1
						if(surviving_regions[1-current_index] != -1) {
							// only tick when both regions are initialized, which
							// is the case after the second round
							COLLECTLATENCY;
							scm_tick_clock(slowClock);
							SHOWLATENCY("scm_tick_clock", pthread_self());
						}
						// tick the fast clock in every round
						COLLECTLATENCY;
						scm_tick_clock(fastClock);
						SHOWLATENCY("scm_tick_clock", pthread_self());
#else
						while (mp < save_start) {
							//printf("1 deallocating from memory address %p\n", mp);
							COLLECTLATENCY;
							free (*mp);
							SHOWLATENCY("free", pthread_self());
                                        		UPDATENETMEM(-memory_sizes[mpindex]);
							mp++;
							mpindex++;
						}
						mp = mpe;
						mpindex = mpeindex;

						while (mp > save_end) {
							mp--;
							mpindex--;
							//printf("2 deallocating from memory address %p\n", mp);
							COLLECTLATENCY;
							free (*mp);
							SHOWLATENCY("free", pthread_self());
                                        		UPDATENETMEM(-memory_sizes[mpindex]);
						}
#endif

#ifdef PRINTMEMINFO
#if STR_MALLOC || STM_MALLOC || STRMC_MALLOC
						while (mpindex < save_start_index) {
                                        		UPDATENETMEM(-memory_sizes[mpindex]);
							mpindex++;
						}
						mpindex = mpeindex;

						while (mpindex > save_end_index) {
							mpindex--;
                                        		UPDATENETMEM(-memory_sizes[mpindex]);
						}
#endif
#endif
						// invalidate old regions, create two new regions
						// and adjust old_buf_start
#if STR_MALLOC || STRMC_MALLOC
						if(surviving_regions[1-current_index] != -1) {
							// if the other region was initialized, unregister it
							COLLECTLATENCY;
							scm_unregister_region(surviving_regions[1-current_index]);
							SHOWLATENCY("scm_unregister_region", pthread_self());
						}
						// update current index
						current_index = 1 - current_index;

						COLLECTLATENCY;
						scm_unregister_region(temporary_region);
						SHOWLATENCY("scm_unregister_region", pthread_self());

						COLLECTLATENCY;
						temporary_region = scm_create_region();
						SHOWLATENCY("scm_create_region", pthread_self());

						COLLECTLATENCY;
						surviving_regions[current_index] = scm_create_region();
						SHOWLATENCY("scm_create_region", pthread_self());

						if(save_end != mpe)
							old_buf_start = save_end;
						else
							old_buf_start = memory;

#endif
#if STR_MALLOC
						COLLECTLATENCY;
						scm_refresh_region(temporary_region, 0);
						SHOWLATENCY("scm_refresh_region", pthread_self());

						COLLECTLATENCY;
						scm_refresh_region(surviving_regions[current_index], 1);
						SHOWLATENCY("scm_refresh_region", pthread_self());
#elif STRMC_MALLOC

						COLLECTLATENCY;
						scm_refresh_region_with_clock(temporary_region, 0, fastClock);
						SHOWLATENCY("scm_refresh_region_with_clock", pthread_self());

						COLLECTLATENCY;
						scm_refresh_region_with_clock(surviving_regions[current_index], 0, slowClock);
						SHOWLATENCY("scm_refresh_region_with_clock", pthread_self());
#endif
#ifdef WITHOUT_LEAK
						if(save_start == memory) {
							mp = save_end;
							mpindex = save_end_index;
						} else {
							mp = memory;
							mpindex = 0;
						}
#else
						mp = memory;
						mpindex = 0;
#endif
						//printf("mp: %p\n", mp);
					}
				} /* end: while (iterations--) */
			} /* end: for (size = size_base; size; size /= 2) */
		} /*end: for (size_base = uMinBlockSize;
				size_base < uMaxBlockSize;
				size_base = size_base * 3 / 2 + 1)*/
	} /* end: while (repeat--) */
	/* free the residual allocations */
	mpe = mp;
	mpeindex = mpindex;
	mp = memory;
	mpindex = 0;

#ifdef MALLOC_ONLY
	while (mp < mpe) {
		//printf("deallocating from memory address %p\n", mp);
		COLLECTLATENCY;
		free (*mp);
		SHOWLATENCY("free", pthread_self());
		UPDATENETMEM(-memory_sizes[mpindex]);
		mp++;
		mpindex++;
	}
#endif
#ifdef PRINTTHROUGHPUT
	fprintf(fout, "Number of objects for thread %d: %u \n", pthread_self(), num_objects);
#endif
	free(memory);
	UPDATENETMEM(-(ulCallCount*sizeof(void*)));
	SHOWMEMINFO;
}

unsigned long promptAndRead(char *msg, unsigned long defaultVal, char fmtCh)
{
	/*char *arg = NULL, *err;
	unsigned long result;
	{
		char buf[12];
		static char fmt[] = "\n%s [%lu]: ";
		fmt[7] = fmtCh;
		fprintf_silent(fout, fmt, msg, defaultVal);
		if (fgets(buf, 11, fin))
			arg = &buf[0];
	}
	if (arg && ((result = strtoul(arg, &err, 10)) != 0
					|| (*err == '\n' && arg != err)))
	{
		return result;
	}
	else*/
	return defaultVal;
}


/*** System-Specific Interfaces ***/

#ifdef SYS_MULTI_THREAD
ThreadID RunThread(void (*fn)(void *), void *arg)
{
	ThreadID result = THREAD_NULL;

#if defined(__OS2__) && (defined(__IBMC__) || defined(__IBMCPP__) || defined(__WATCOMC__))
	if ((result = _beginthread(fn, NULL, 8192, arg)) == THREAD_NULL)
		return THREAD_NULL;

#elif (defined(__OS2__) && defined(__BORLANDC__)) || defined(WIN32)
	if ((result = (ThreadID)_beginthread(fn, 8192, arg)) == THREAD_NULL)
		return THREAD_NULL;

#elif defined(__sun)
	/*	thr_setconcurrency(2); */
	return thr_create(NULL, 0, (void *(*)(void *))fn, arg, THR_BOUND, NULL)==0;

#elif defined(_DECTHREADS_) && !defined(__osf__)
	if (pthread_create(&result, pthread_attr_default,
			(pthread_startroutine_t)fn, arg) == -1)
		return THREAD_NULL;

#elif defined(_POSIX_THREADS) || defined(_POSIX_REENTRANT_FUNCTIONS)
	pthread_attr_t attr;
	pthread_attr_init(&attr);
#ifdef _AIX
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_UNDETACHED);
#endif /* _AIX */
	if (pthread_create(&result, &attr, (void *(*)(void *))fn, arg) == -1)
		return THREAD_NULL;

#else
#error Unsupported threads package
#endif

	return result;
}

/* wait for all benchmark threads to terminate */
void WaitForThreads(ThreadID tids[], unsigned tidCnt)
{
#ifdef __OS2__
	while (tidCnt--)
		DosWaitThread(&tids[tidCnt], DCWW_WAIT);

#elif defined(WIN32)
	WaitForMultipleObjects(tidCnt, tids, TRUE, INFINITE);

#elif defined(__sun)
	int prio;
	thr_getprio(thr_self(), &prio);
	thr_setprio(thr_self(), max(0, prio-1));
	while (tidCnt--)
		thr_join(0, NULL, NULL);

#elif defined(_POSIX_THREADS) || defined(_POSIX_REENTRANT_FUNCTIONS)
	while (tidCnt--)
		pthread_join(tids[tidCnt], NULL);

#endif
}

#ifdef __hpux
#include <sys/pstat.h>
/*#include <sys/mpctl.h>*/
#elif defined(__DGUX__)
#include <sys/dg_sys_info.h>
#endif

/* return the number of processors present */
int GetNumProcessors()
{
#ifdef WIN32
	SYSTEM_INFO info;

	GetSystemInfo(&info);

	return info.dwNumberOfProcessors;

#elif defined(UNIX)

#ifdef __hpux
	struct pst_dynamic psd;
	if (pstat_getdynamic(&psd, sizeof(psd), 1, 0) != -1)
		return psd.psd_proc_cnt;
	/*	shi_bSMP mpctl(MPC_GETNUMSPUS, 0, 0) > 1; */

#elif defined(sgi)
	return sysconf(_SC_NPROC_CONF);
#elif defined(__DGUX__)
	struct dg_sys_info_pm_info info;

	if (dg_sys_info((long *)&info, DG_SYS_INFO_PM_INFO_TYPE,
			DG_SYS_INFO_PM_CURRENT_VERSION)
			== 0)
		return info.idle_vp_count;
	else
		return 0;
#elif defined(__linux__)
	return get_nprocs();
#elif defined(_SC_NPROCESSORS_ONLN)
	return sysconf(_SC_NPROCESSORS_ONLN);

#elif defined(_SC_NPROCESSORS_CONF)
	return sysconf(_SC_NPROCESSORS_CONF);
#endif

#endif

	return 1;
}
#endif /* SYS_MULTI_THREAD */
