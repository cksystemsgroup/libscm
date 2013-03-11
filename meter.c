/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#include "meter.h"

#ifdef SCM_PRINTMEM

#ifdef SCM_PRINTOVERHEAD
static long mem_overhead = 0 ;

void inc_overhead(long inc) {
    //mem_overhead += inc;
    __sync_add_and_fetch(&mem_overhead, inc);
}

void dec_overhead(long inc) {
    //mem_overhead -= inc;
    __sync_sub_and_fetch(&mem_overhead, inc);
}
#endif

static long alloc_mem = 0;
static long num_alloc = 0;

/**
 * Keeps track of the allocated memory
 */
void inc_allocated_mem(long inc) {
    //alloc_mem += inc;
    __sync_add_and_fetch(&alloc_mem, inc);
    __sync_add_and_fetch(&num_alloc, 1);
}

static long freed_mem = 0;
static long num_freed = 0;

/**
 * Keeps track of the freed memory
 */
void inc_freed_mem(long inc) {
    //freed_mem += inc;
    __sync_add_and_fetch(&freed_mem, inc);
    __sync_add_and_fetch(&num_freed, 1);
}

static long pooled_mem = 0;

/**
 * Keep track of the pooled memory
 */
void inc_pooled_mem(long inc) {
    //pooled_mem += inc;
    __sync_add_and_fetch(&pooled_mem, inc);
}

void dec_pooled_mem(long inc) {
    //pooled_mem -= inc;
    __sync_sub_and_fetch(&pooled_mem, inc);
}

static long needed_mem = 0;

/**
 * Keeps track of the needed memory
 */
void inc_needed_mem(long inc) {
    //needed_mem += inc;
    __sync_add_and_fetch(&needed_mem, inc);
}

static long not_needed_mem = 0;

/**
 * Keeps track of the not-needed memory
 */
void inc_not_needed_mem(long inc) {
    //not_needed_mem += inc;
    __sync_add_and_fetch(&not_needed_mem, inc);
}

static long start_time = 0;

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

    // struct mallinfo info = mallinfo();

    long used = alloc_mem - pooled_mem;
    long needed = needed_mem - not_needed_mem;

    if (needed < 0)
        needed = 0;

    printf("memusage:\t%lu\t%ld\t%ld\t%ld\t%ld\n", usec - start_time, alloc_mem - freed_mem, pooled_mem, used, needed);

#ifdef SCM_PRINTOVERHEAD
    printf("memoverhead:\t%lu\t%lu\n", usec - start_time, mem_overhead);
#endif //SCM_PRINTOVERHEAD

    // printf("mallinfo:\t%lu\t%d\n", usec - start_time, info.uordblks);
}

#endif  /* SCM_PRINTMEM */