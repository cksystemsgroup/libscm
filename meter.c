/*
 * Copyright (c) 2010 Martin Aigner, Andreas Haas
 * http://cs.uni-salzburg.at/~maigner
 * http://cs.uni-salzburg.at/~ahaas
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
#include "stm-debug.h"
#include "meter.h"

//#ifdef SCM_CALCMEM
#include <malloc.h>
//#endif //SCM_CALCMEM

//#ifdef SCM_CALCOVERHEAD
long mem_overhead __attribute__ ((visibility("hidden"))) = 0 ;

void inc_overhead(long inc) {
    //mem_overhead += inc;
    __sync_add_and_fetch(&mem_overhead, inc);
    
}
void dec_overhead(long inc) {
    //mem_overhead -= inc;
    __sync_sub_and_fetch(&mem_overhead, inc);
}
//#endif

//#ifdef SCM_CALCMEM
static int mem_meter_enabled = 1;
static long start_time = 0;
void enable_mem_meter() {
    mem_meter_enabled = 1;
}
void disable_mem_meter() {
    mem_meter_enabled = 0;
}

static long freed_mem = 0;
static long alloc_mem = 0;
static long num_freed = 0;
static long num_alloc = 0;

void inc_freed_mem(long inc) {
    //freed_mem += inc;
    __sync_add_and_fetch(&freed_mem, inc);
    __sync_add_and_fetch(&num_freed, 1);
}
void inc_allocated_mem(long inc) {
    //used_mem += inc;
    __sync_add_and_fetch(&alloc_mem, inc);
    __sync_add_and_fetch(&num_alloc, 1);
}

void print_memory_consumption() {

    struct timeval t;
    gettimeofday(&t, NULL);

    long usec = t.tv_sec * 1000000 + t.tv_usec;

    if (start_time == 0) {
        start_time = usec;
    }

    struct mallinfo info = mallinfo();

    if (mem_meter_enabled != 0) {
        printf("memusage:\t%lu\t%ld\n", usec - start_time, alloc_mem - freed_mem);
        printf("memoverhead:\t%lu\t%lu\n", usec - start_time, mem_overhead);
        printf("mallinfo:\t%lu\t%d\n", usec - start_time, info.uordblks);
    }
}

void scm_get_mem_info(struct scm_mem_info *info) {
	info->allocated = alloc_mem;
	info->freed = freed_mem;
	info->overhead = mem_overhead;
        info->num_alloc = num_alloc;
        info->num_freed = num_freed;
}
//#endif //SCM_CALCMEM
