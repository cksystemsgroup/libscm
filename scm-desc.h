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

#ifndef _SCM_DESC_H
#define	_SCM_DESC_H

#include <stdlib.h>
#include <pthread.h>
#include "stm.h"
#include "arch.h"
#include <errno.h>
#include <limits.h>

#include "regmalloc.h"

#ifdef SCM_MT_DEBUG
#define printf printf("%lu: ", pthread_self());printf
#endif

#ifdef SCM_MAKE_MICROBENCHMARKS
#define MICROBENCHMARK_START unsigned long long _mb_start = rdtsc();
#define MICROBENCHMARK_STOP unsigned long long _mb_stop = rdtsc();
#define MICROBENCHMARK_DURATION(_location) \
    printf("microbenchmark_at_%s:\t%llu\n", _location, (_mb_stop-_mb_start));
#else
#define MICROBENCHMARK_START //NOOP
#define MICROBENCHMARK_STOP //NOOP
#define MICROBENCHMARK_DURATION(_location) //NOOP
#endif

#ifndef SCM_DESCRIPTORS_PER_PAGE
#define SCM_DESCRIPTORS_PER_PAGE \
        ((SCM_DESCRIPTOR_PAGE_SIZE - 2 * sizeof(void*))/sizeof(void*))
#endif

#ifndef SCM_MAX_REGIONS
#define SCM_MAX_REGIONS 10
#endif

#ifndef SCM_MAX_CLOCKS
#define SCM_MAX_CLOCKS 10
#endif

#ifndef SCM_REGION_PAGE_FREELIST_SIZE
#define SCM_REGION_PAGE_FREELIST_SIZE 10
#endif

#define HB_MASK (UINT_MAX - INT_MAX)

#define CACHEALIGN(x) (ROUND_UP(x,8))
#define ROUND_UP(x,y) (ROUND_DOWN(x+(y-1),y))
#define ROUND_DOWN(x,y) (x & ~(y-1))

extern void* __real_malloc(size_t size);
extern void* __real_calloc(size_t nelem, size_t elsize);
extern void* __real_realloc(void *ptr, size_t size);
extern void __real_free(void *ptr);
extern size_t __real_malloc_usable_size(void *ptr);

/*
 * objects allocated using libscm haven an additional object header that
 * is added before the chunk returned by the malloc implemented in the
 * standard library.
 *
 * -------------------------  <- pointer to object_header_t
 * | descriptor counter dc |
 * -------------------------  <- pointer to the payload data that is
 * | payload data          |     returned to the user
 * ~ returned by malloc    ~
 * |                       |
 * -------------------------
 *
 */
#define OBJECT_HEADER(_ptr) \
    (object_header_t*)(_ptr - sizeof (object_header_t))
#define PAYLOAD_OFFSET(_o) \
    ((void*)(_o) + sizeof(object_header_t))

typedef struct object_header object_header_t;
typedef struct descriptor_page_list descriptor_page_list_t;
typedef struct expired_descriptor_page_list expired_descriptor_page_list_t;
typedef struct descriptor_page descriptor_page_t;
typedef struct descriptor_buffer descriptor_buffer_t;

#ifndef SCM_TEST
typedef struct descriptor_root descriptor_root_t;
descriptor_root_t *get_descriptor_root(void)
__attribute__((visibility("hidden")));
#endif

// The descriptor root is stored as thread-local storage variable.
// According to perf tools from google __thread is faster than pthread_getspecific().
extern __thread descriptor_root_t* descriptor_root;

struct object_header {
    // depending on whether the region-based allocator is
    // used or not, the following field will be positive or negative.
    // A negative value indicates region allocation. Switching the
    // lsb will return the region id.
    int dc_or_region_id;
    // finalizer_index must be signed so that a finalizer_index
    // may be set to -1 indicating that no finalizer exists
    int finalizer_index;
};

struct descriptor_page_list {
    descriptor_page_t* first;
    descriptor_page_t* last;
};

struct expired_descriptor_page_list {
    descriptor_page_t* first;
    descriptor_page_t* last;
    unsigned long collected;
};

/*
 * statically allocate memory for the locally clocked descriptor buffers
 * size of the locally clocked buffer is SCM_MAX_EXPIRATION_EXTENSION + 1
 * because of the additional slots for
 * 1. slot for the current time
 *
 * statically allocate memory for the globally clocked descriptor buffers
 * size of the globally clocked buffer is SCM_MAX_EXPIRATION_EXTENSION + 2
 * because of the additional slots for
 *  1. slot for the current time
 *  2. adding descriptors at current + increment + 1
 *
 * Note: both buffers allocate SCM_MAX_EXPIRATION_EXTENSION + 2 slots for
 * page_lists but the locally clocked buffer uses only
 * SCM_MAX_EXPIRATION_EXTENSION + 1 slots
 */
struct descriptor_buffer {
    descriptor_page_list_t not_expired[SCM_MAX_EXPIRATION_EXTENSION + 2];
    // The field not_expired_length may have the following values:
    //		0 : indicates that the descriptor buffer is unused
    //		SCM_MAX_EXPIRATION_EXTENSION + 1 : indicates that the descriptor
    //				buffer is used as thread-local clock, and
    //		SCM_MAX_EXPIRATION_EXTENSION + 2 : indicates that the descriptor
    //				buffer is used as global clock.
    unsigned int not_expired_length;
    unsigned int current_index;
    // status: age != current_time => zombie,
    // Initially, all descriptor buffers but the first one are zombies
    // (because register thread increments the current_time)
    unsigned int age;
};

/**
 * Descriptor root holds thread-local data for descriptor
 * and region management.
 */
struct descriptor_root {
    unsigned long global_phase;

    expired_descriptor_page_list_t list_of_expired_obj_descriptors;
    expired_descriptor_page_list_t list_of_expired_reg_descriptors;

    descriptor_buffer_t globally_clocked_obj_buffer;
    descriptor_buffer_t globally_clocked_reg_buffer;

    descriptor_buffer_t locally_clocked_obj_buffer[SCM_MAX_CLOCKS];
    descriptor_buffer_t locally_clocked_reg_buffer[SCM_MAX_CLOCKS];
    unsigned int next_clock_index;
    // The following field indicates the time when the thread was created
    // the field is necessary to distinguish zombie descriptor buffers
    // from currently used descriptor buffers.
    // Initially, all descriptor buffers but the first one are zombies
    // (because register thread increments the current_time)
    unsigned int current_time;
    // The round_robin field is an index of the locally_clocked buffers
    // which constantly increases modulo SCM_MAX_CLOCKS - 1.
    // round_robin is never set to 0 because the first locally_clocked
    // buffer is the base clock of the thread and can never be a
    // zombie buffer.
    // round_robin is used to enable the constant-time cleaning of
    // zombie buffers.
    unsigned int round_robin;

    descriptor_page_t* descriptor_page_pool[SCM_DESCRIPTOR_PAGE_FREELIST_SIZE];
    unsigned long number_of_pooled_descriptor_pages;

    region_t regions[SCM_MAX_REGIONS];
    unsigned int next_reg_index;

    region_page_t* region_page_pool;
    unsigned long number_of_pooled_region_pages;

    //used to build a list of terminated descriptor_roots
    descriptor_root_t *next;
};

struct descriptor_page {
    descriptor_page_t *next;
    unsigned long number_of_descriptors;
    object_header_t* descriptors[SCM_DESCRIPTORS_PER_PAGE];
};

/* expire_reg_descriptor_if_exists()
 * expires object descriptors */
int expire_obj_descriptor_if_exists(expired_descriptor_page_list_t *list)
__attribute__((visibility("hidden")));

/* expire_reg_descriptor_if_exists()
 * expires region descriptors */
int expire_reg_descriptor_if_exists(expired_descriptor_page_list_t *list)
__attribute__((visibility("hidden")));

/* Takes an object or a region as parameter ptr */
void insert_descriptor(void* ptr,
                       descriptor_buffer_t *buffer, unsigned int expiration)
__attribute__((visibility("hidden")));

/* Expires the descriptor buffer by appending
 * the just-expired descriptors to the
 * list_of_expired_[obj|reg]_descriptors */
void expire_buffer(descriptor_buffer_t *buffer,
                   expired_descriptor_page_list_t *exp_list)
__attribute__((visibility("hidden")));

inline void increment_current_index(descriptor_buffer_t *buffer)
__attribute__((visibility("hidden")));

int run_finalizer(object_header_t *o);

#endif	/* _SCM_DESC_H */
