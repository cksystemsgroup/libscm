/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _SCM_H_
#define	_SCM_H_

#include <stdio.h>

#include <pthread.h>
#include <limits.h>

//#include <stdlib.h>
//#include <errno.h>

#include "arch.h"
#include "object.h"
#include "descriptors.h"
#include "libscm.h"

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

#define HB_MASK (UINT_MAX - INT_MAX)

#define CACHEALIGN(x) (ROUND_UP(x,8))
#define ROUND_UP(x,y) (ROUND_DOWN(x+(y-1),y))
#define ROUND_DOWN(x,y) ((x) & ~(y-1))

#endif	/* _SCM_H_ */