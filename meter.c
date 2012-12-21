/*
 * Copyright (c) 2010 Martin Aigner, Andreas Haas, Stephanie Stroka
 * http://cs.uni-salzburg.at/~maigner
 * http://cs.uni-salzburg.at/~ahaas
 * http://cs.uni-salzburg.at/~sstroka
 *
 * University Salzburg, www.uni-salzburg.at
 * Department of Computer Science, cs.uni-salzburg.at
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <sys/time.h>
#include "stm.h"
#include "meter.h"

#ifdef SCM_PRINTMEM
#include <malloc.h>
#endif

#ifdef SCM_PRINTOVERHEAD
long mem_overhead __attribute__ ((visibility("hidden"))) = 0 ;

void inc_overhead(long inc) {
    mem_overhead += inc;
}
void dec_overhead(long inc) {
    mem_overhead -= inc;
}
#endif

#ifdef SCM_PRINTMEM
static int mem_meter_enabled = 1;
static long start_time = 0;
void enable_mem_meter() {
    mem_meter_enabled = 1;
}
void disable_mem_meter() {
    mem_meter_enabled = 0;
}

static int freed_mem = 0;
static int allocated_mem = 0;
static int pooled_mem = 0;
static int not_needed_mem = 0;
static int needed_mem = 0;
static int firstTime = 1;


void inc_pooled_mem(int inc) {
    pooled_mem += inc;
}

void dec_pooled_mem(int inc) {
    pooled_mem -= inc;
}

/**
 * Keeps track of the freed memory
 */
void inc_freed_mem(int inc) {
    freed_mem += inc;
}

/**
 * Keeps track of the allocated memory
 */
void inc_allocated_mem(int inc) {
    allocated_mem += inc;
}

/**
 * Keeps track of the not needed memory
 */
void inc_not_needed_mem(int inc) {
    not_needed_mem += inc;
}

/**
 * Keeps track of the needed memory
 */
void inc_needed_mem(int inc) {
    needed_mem += inc;
}

/**
 * prints the memory consumption for needed and allocated memory
 */
void print_memory_consumption() {

    struct timeval t;
    gettimeofday(&t, NULL);

    long usec = t.tv_sec * 1000000 + t.tv_usec;

    if (start_time == 0) {
        start_time = usec;
    }

    struct mallinfo info = mallinfo();

    if (mem_meter_enabled != 0) {
        FILE* f;
        if (firstTime) {
            f = fopen("data.dat", "w");
        } else {
            f = fopen("data.dat", "a");
        }
        int needed = needed_mem - not_needed_mem;
        int used = allocated_mem - pooled_mem;
        if (needed < 0)
            needed = 0;
        fprintf(f, "%lu\t%d\t%d\t%d\t%d\n", usec - start_time, allocated_mem - freed_mem, pooled_mem, used, needed);
        fclose(f);
        printf("memusage:\t%lu\t%d\t%d\t%d\t%d\n", usec - start_time, allocated_mem - freed_mem, pooled_mem, used, needed);
#ifdef SCM_PRINTOVERHEAD
        printf("memoverhead:\t%lu\t%lu\n", usec - start_time, mem_overhead);
#endif //SCM_PRINTOVERHEAD
        printf("mallinfo:\t%lu\t%d\n", usec - start_time, info.uordblks);
    }
}
#endif
