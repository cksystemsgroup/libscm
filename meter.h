/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _METER_H
#define	_METER_H

//#ifdef SCM_CALCOVERHEAD
    void inc_overhead(long inc) __attribute__ ((visibility("hidden")));
    void dec_overhead(long inc) __attribute__ ((visibility("hidden")));
//#endif

//#ifdef SCM_CALCMEM

    void inc_freed_mem(long inc) __attribute__ ((visibility("hidden")));
    void inc_allocated_mem(long inc) __attribute__ ((visibility("hidden")));
    void enable_mem_meter(void) __attribute__ ((visibility("hidden")));
    void disable_mem_meter(void) __attribute__ ((visibility("hidden")));
    void print_memory_consumption(void) __attribute__ ((visibility("hidden")));
//#endif

#endif	/* _METER_H */
