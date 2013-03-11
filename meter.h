/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _METER_H_
#define	_METER_H_

#include <stdio.h>
#include <sys/time.h>

#ifdef SCM_PRINTMEM

#ifdef SCM_PRINTOVERHEAD
void inc_overhead(long inc) __attribute__((visibility("hidden")));
void dec_overhead(long inc) __attribute__((visibility("hidden")));
#endif

/**
 * Keeps track of the allocated memory
 */
void inc_allocated_mem(long inc);

/**
 * Keeps track of the freed memory
 */
void inc_freed_mem(long inc);

/**
 * Keep track of the pooled/unpooled memory
 */
void inc_pooled_mem(long inc);
void dec_pooled_mem(long inc);

/**
 * Keeps track of the needed memory
 */
void inc_needed_mem(long inc);

/**
 * Keeps track of the not needed memory
 */
void inc_not_needed_mem(long inc);

/**
 * prints the memory consumption for needed and allocated memory
 */
void print_memory_consumption(void);

#endif  /* SCM_PRINTMEM */

#endif	/* _METER_H_ */

