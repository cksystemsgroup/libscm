/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _METER_H_
#define	_METER_H_

#ifdef SCM_RECORD_MEMORY_USAGE

#include <stdio.h>
#include <sys/time.h>

#include "debug.h"

/**
 * Keeps track of the allocated memory
 */
void inc_allocated_mem(long inc) __attribute__((visibility("hidden")));

/**
 * Keeps track of the freed memory
 */
void inc_freed_mem(long inc) __attribute__((visibility("hidden")));

/**
 * Keeps track of the pooled memory
 */
void inc_pooled_mem(long inc) __attribute__((visibility("hidden")));
void dec_pooled_mem(long inc) __attribute__((visibility("hidden")));

/**
 * Keeps track of the memory overhead
 */
void inc_overhead(long inc) __attribute__((visibility("hidden")));
void dec_overhead(long inc) __attribute__((visibility("hidden")));

/**
 * Prints memory consumption
 */
void print_memory_consumption(void) __attribute__((visibility("hidden")));

#endif  /* SCM_RECORD_MEMORY_USAGE */

#endif	/* _METER_H_ */