/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _SCM_DESC_H
#define	_SCM_DESC_H

#include <stdlib.h>
#include <pthread.h>

#include "stm.h"
#include "arch.h"

#ifdef SCM_MT_DEBUG
#define printf printf("%lu: ", pthread_self());printf
#endif

#ifdef SCM_MAKE_MICROBENCHMARKS
#define MICROBENCHMARK_START \
        allocator_overhead = 0; \
        unsigned long long _mb_start = rdtsc();
#define MICROBENCHMARK_STOP unsigned long long _mb_stop = rdtsc();
#define MICROBENCHMARK_DURATION(_location) \
        printf("tid: %lu microbenchmark_at_%s:\t%llu\n", \
        pthread_self(), \
        _location, \
        allocator_overhead == 0 ? \
        (_mb_stop-_mb_start) : \
        ((_mb_stop-_mb_start) - allocator_overhead)); \
        allocator_overhead = 0;
#define OVERHEAD_START unsigned long long _overhead_start = rdtsc();
#define OVERHEAD_STOP \
        unsigned long long _overhead_stop = rdtsc(); \
        allocator_overhead += (_overhead_stop - _overhead_start);

#else
#define MICROBENCHMARK_START //NOOP
#define MICROBENCHMARK_STOP //NOOP
#define MICROBENCHMARK_DURATION(_location) //NOOP
#define OVERHEAD_START //NOOP
#define OVERHEAD_STOP //NOOP
#endif

#ifndef SCM_DESCRIPTORS_PER_PAGE
#define SCM_DESCRIPTORS_PER_PAGE \
        ((SCM_DESCRIPTOR_PAGE_SIZE - 2 * sizeof(void*))/sizeof(void*))
#endif

extern void *__real_malloc(size_t size);
extern void *__real_calloc(size_t nelem, size_t elsize);
extern void *__real_realloc(void *ptr, size_t size);
extern void __real_free(void *ptr);
extern size_t __real_malloc_usable_size(void *ptr);

/*
 * objects allocated using libscm haven an additional object header that
 * is added before the chunk returned by the malloc implemented in the
 * standard library.
 *
 * --------------------------------  <- pointer to object_header_t
 * | 32 bit descriptor counter dc |
 * | 32 bit finalizer index       |
 * --------------------------------  <- pointer to the payload data that is
 * | payload data                 |     returned to the user
 * ~ returned to user             ~
 * |                              |
 * --------------------------------
 *
 */
#define OBJECT_HEADER(_ptr) \
    (object_header_t*)(_ptr - sizeof (object_header_t))
#define PAYLOAD_OFFSET(_o) \
    ((void*)(_o) + sizeof(object_header_t))

typedef struct descriptor_root descriptor_root_t;
typedef struct object_header object_header_t;
typedef struct descriptor_page_list descriptor_page_list_t;
typedef struct expired_descriptor_page_list expired_descriptor_page_list_t;
typedef struct descriptor_page descriptor_page_t;
typedef struct descriptor_buffer descriptor_buffer_t;

struct object_header {
    unsigned int dc; /* number of descriptors pointing here */
    int finalizer_index; /* identifier of a finalizer function */
};

/* 
 * singly linked list of descriptor pages
 */
struct descriptor_page_list {
    descriptor_page_t *first;
    descriptor_page_t *last;
};

/*
 * singly linked list of expired descriptor pages
 */
struct expired_descriptor_page_list {
    descriptor_page_t *first;
    descriptor_page_t *last;

    /* index of the first descriptor in first descriptor page. This is the
     * descriptor that will we processed upon expiration */
    long begin;
};

/*
 * statically allocate memory for the locally clocked descriptor buffers
 * size of the locally clocked buffer is SCM_MAX_EXPIRATION_EXTENSION + 1
 * because of the additional slots for
 * 1. slot for the current time
 *
 * statically allocate memory for the globally clocked descriptor buffers
 * size of the globally clocked buffer is SCM_MAX_EXPIRATION_EXTENSION + 3
 * because of the additional slots for
 *  1. slot for the current time
 *  2. adding descriptors at current + increment + 1
 *  3. removing descriptors from current - 1
 *
 * Note: both buffers allocate SCM_MAX_EXPIRATION_EXTENSION + 3 slots for
 * page_lists but the locally clocked buffer uses only
 * SCM_MAX_EXPIRATION_EXTENSION + 1 slots
 */
struct descriptor_buffer {
    /* for every possible expiration extension, there is an array element
     * in "not_expired" that contains a descriptor page list where the 
     * descriptor is stored in */
    descriptor_page_list_t not_expired[SCM_MAX_EXPIRATION_EXTENSION + 3];

    /* "not_expired_length" gives the length of the "not_expired" array */
    unsigned int not_expired_length;

    /* "current_index" is a index to the descriptor_page_list in
     * "not_expired" that will expire after the next tick.*/
    unsigned int current_index;
};

/* 
 * A collection of data structures. Each thread has a pointer to 
 * a "descriptor_root" in a thread-specific data slot
 */
struct descriptor_root {
    /* "global_phase" indicates if the thread has already ticked in the actual 
     * global phase. A global phase is the interval between two increments of
     * the global clock
     * 
     * global_phase == global time => thread has not ticked yet 
     * global_phase == global time +1 => thread has already ticked at least once
     */
    long global_phase;

    expired_descriptor_page_list_t list_of_expired_descriptors;

    /* globally_clocked_buffer->current_index is the thread-global time */
    descriptor_buffer_t globally_clocked_buffer;

    /* locally_clocked_buffer->current_index is the thread-local time */
    descriptor_buffer_t locally_clocked_buffer;

    /* a pool of descriptor pages for re-use */
    descriptor_page_t * descriptor_page_pool[SCM_DESCRIPTOR_PAGE_FREELIST_SIZE];
    long number_of_pooled_descriptor_pages;

    /* used to build a list of terminated descriptor_roots. This is only
     * used after the thread terminated */
    descriptor_root_t *next;
};

/*
 * a chunk of contiguous memory that holds a set of descriptors with the same
 * expiration date.
 */
struct descriptor_page {
    descriptor_page_t* next; /* used to build a linked list*/
    unsigned long number_of_descriptors; /* utilization of descriptors[] */
    object_header_t * descriptors[SCM_DESCRIPTORS_PER_PAGE]; /* memory area */
};

descriptor_root_t *get_descriptor_root(void)
    __attribute__((visibility("hidden")));

int expire_descriptor_if_exists(expired_descriptor_page_list_t *list)
    __attribute__((visibility("hidden")));

void insert_descriptor(object_header_t *o,
    descriptor_buffer_t *buffer, unsigned int expiration)
    __attribute__((visibility("hidden")));

void expire_buffer(descriptor_buffer_t *buffer,
    expired_descriptor_page_list_t *exp_list)
    __attribute__((visibility("hidden")));

inline void increment_current_index(descriptor_buffer_t *buffer)
    __attribute__((visibility("hidden")));

int run_finalizer(object_header_t *o);

#endif	/* _SCM_DESC_H */
