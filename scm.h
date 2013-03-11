/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _SCM_H_
#define	_SCM_H_

#include <stdio.h>

/*
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
*/

#include "libscm.h"
#include "arch.h"
#include "regions.h"

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

#ifndef SCM_MAX_REGIONS
#define SCM_MAX_REGIONS 10
#endif

#ifndef SCM_MAX_CLOCKS
#define SCM_MAX_CLOCKS 10
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
 * objects allocated through libscm have an additional object header that
 * is located before the chunk storing the object.
 *
 * -------------------------  <- pointer to object_header_t
 * | descriptor counter OR |
 * | region id AND         |
 * | finalizer index       |
 * -------------------------  <- pointer to the payload data that is
 * | payload data          |     returned to the user
 * ~ returned to user      ~
 * |                       |
 * -------------------------
 *
 */
typedef struct object_header object_header_t;

struct object_header {
    // Depending on whether the region-based allocator is
    // used or not, the following field will be positive or negative.
    // A negative value indicates region allocation. Resetting the
    // hsb returns the region id.
    int dc_or_region_id;
    // finalizer_index must be signed so that a finalizer_index
    // may be set to -1 indicating that no finalizer exists
    int finalizer_index;
};

#define OBJECT_HEADER(_ptr) \
    (object_header_t*)(_ptr - sizeof(object_header_t))
#define PAYLOAD_OFFSET(_o) \
    ((void*)(_o) + sizeof(object_header_t))

#endif	/* _SCM_H_ */
