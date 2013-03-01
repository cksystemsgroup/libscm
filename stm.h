/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _STM_H
#define	_STM_H

#include <stddef.h>

/*
 * one may use the following compile time configuration for libscm.
 * See Makefile for different configurations
 *
 * check pre-/post-conditions on stm functions
 * #define SCM_TEST
 *
 * turn on debug messages
 * #define SCM_DEBUG
 *
 * add thread id to debug messages
 * #define SCM_MT_DEBUG
 *
 * print memory consumption after memory operations
 * #define SCM_PRINTMEM
 *
 * print bookkeeping memory overhead.
 * works only in addition with SCM_PRINTMEM
 * #define SCM_PRINTOVERHEAD
 *
 * print information if contention on locks happened
 * #define SCM_PRINTLOCK
 *
 * the maximal expiration extension allowed on the scm_refresh calls
 * #define SCM_MAX_EXPIRATION_EXTENSION 5
 *
 * the size of the descriptor pages. this should be a power of two and a
 * multiple of sizeof(void*)
 * #define SCM_DESCRIPTOR_PAGE_SIZE 4096
 * the SCM_DESCRIPTOR_PAGE_SIZE results in SCM_DESCRIPTORS_PER_PAGE equal to
 *   ((SCM_DESCRIPTOR_PAGE_SIZE - 2 * sizeof(void*))/sizeof(void*))
 *
 * an upper bound on the number of descriptor pages that are cached
 * #define SCM_DESCRIPTOR_PAGE_FREELIST_SIZE 10
 *
 * print the number of cpu cycles for each public function. Make shure to NOT
 * enable any other debug options together with SCM_MAKE_MICROBENCHMARKS
 * #define SCM_MAKE_MICROBENCHMARKS
 */

/*
 * default configuration
 */
#ifndef SCM_DESCRIPTOR_PAGE_SIZE
#define SCM_DESCRIPTOR_PAGE_SIZE 4096
#endif

#ifndef SCM_MAX_EXPIRATION_EXTENSION
#define SCM_MAX_EXPIRATION_EXTENSION 10
#endif

#ifndef SCM_DESCRIPTOR_PAGE_FREELIST_SIZE
#define SCM_DESCRIPTOR_PAGE_FREELIST_SIZE 10
#endif

#ifndef SCM_REGION_PAGE_FREELIST_SIZE
#define SCM_REGION_PAGE_FREELIST_SIZE 10
#endif

#ifdef SCM_TEST
typedef struct descriptor_root descriptor_root_t;
extern descriptor_root_t* get_descriptor_root(void);
#endif

/**
 * scm_create_region() returns a const integer representing a new region index
 * if available and -1 otherwise. The new region is detected by scanning
 * the descriptor_root regions array for a region that does not yet have 
 * any region_page, using a next-fit strategy. If such a region is found,
 * a region_page is created and initialized.
 */
extern const int scm_create_region();

/**
 * scm_unregister_region() sets the region age back to a value that is not equal
 * to the descriptor_root current_time. As a consequence the region may
 * be reused again if the dc is 0.
 */
extern void scm_unregister_region(const int region);

/**
 * scm_register_clock() returns a const integer representing
 * a new clock in the short-term memory model.
 * A clock identifies a descriptor buffer in the array of locally
 * clocked descriptor buffers of the descriptor root.
 * If all available clocks/descriptor buffers are in use, the return value is
 * set to -1, indicating an error for the caller function.
 */
extern const int scm_register_clock();

/**
 * scm_unregister_clock() sets the descriptor buffer age back to a 
 * value that is not equal to the descriptor_root current_time. 
 * As a consequence the clock buffer will be cleaned up incrementally 
 * during scm_tick() calls.
 */
extern void scm_unregister_clock(const int clock);

/**
 * scm_malloc_in_region() allocates memory in a region.
 * scm_malloc_in_region() wraps an object header around
 * objects allocated in a region. The object header allows to
 * "redirect" a refresh call to a region, if a region object
 * is refreshed.
 */
extern void* scm_malloc_in_region(size_t size, const int region_index);

/**
 * scm_malloc() allocates short-term memory objects. This function
 * can be used at compile time. Unmodified code which uses e.g. glibc's
 * malloc can be used with linker option --wrap malloc
 */
void *scm_malloc(size_t size);

/**
 * scm_free() frees short-term memory objects with no descriptors on
 * them e.g. permanent objects. This function can be used at compile time.
 * Unmodified code which uses e.g. glibc's free can be used with linker
 * option --wrap free
 */
void scm_free(void *ptr);

/**
 * scm_tick() advances the local time of the calling thread
 */
void scm_tick(void);

/**
 * scm_tick_clock() advances the time of the given thread-local clock
 */
void scm_tick_clock(const unsigned long clock);

/**
 * scm_global_tick() advances the global time of the calling thread
 */
void scm_global_tick(void);

/**
 * scm_refresh() adds extension time units to the expiration time of
 * ptr without taking care of other threads.
 * In a multi-clock environment, scm_refresh refreshes
 * the object with the thread-local base clock.
 * If the object is part of a region, the region is refreshed instead.
 */
void scm_refresh(void *ptr, unsigned int extension);

/**
 * scm_refresh_region() adds extension time units to the expiration time of
 * a region without taking care of other threads.
 * In a multi-clock environment, scm_refresh refreshes
 * the region with the thread-local base clock.
 */
void scm_refresh_region(const int region_id, unsigned int extension);

/**
 * scm_refresh_with_clock() refreshes a given object with a given clock,
 * which can be different to the thread-local base clock.
 * If an object is refreshed with multiple clocks it lives
 * until all clocks ticked n times, where n is the respective extension.
 * If the object is part of a region, the region is refreshed instead.
 */
void scm_refresh_with_clock(void *ptr, unsigned int extension, const unsigned long clock);

/**
 * scm_refresh_region_with_clock() refreshes a given region with a given
 * clock, which can be different from the thread-local base clock.
 * If a region is refreshed with multiple clocks it lives
 * until all clocks ticked n times, where n is the respective extension.
 */
void scm_refresh_region_with_clock(const int region_id, unsigned int extension, const unsigned long clock);

/**
 * scm_global_refresh() adds extension time units to the expiration time of
 * ptr and takes care that all other threads have enough time to also call
 * scm_global_refresh(ptr, extension). If the object is part of a region, the
 * region is refreshed instead.
 */
void scm_global_refresh(void *ptr, unsigned int extension);

/**
 * scm_global_refresh_region() adds extension time units to the expiration time of
 * a region and takes care that all other threads have enough time to also call
 * scm_global_refresh_region(region_id, extension).
 */
void scm_global_refresh_region(const int region_id, unsigned int extension);

/**
 * scm_unregister_thread() may be called just before a thread terminates.
 * The thread's data structures are preserved for a new thread to join
 * the short-term memory system. Registration of a thread is done
 * automatically when a thread calls *_tick or *_refresh the first time.
 */
void scm_unregister_thread(void);

/**
 * scm_block_thread() signals the short-term memory system that
 * the calling thread is about to leave the system for a while e.g. because of
 * a blocking call. During this period the system does not wait for scm_tick
 * calls of this thread.
 * After the thread finished the blocking state it re-joins the short-term
 * memory system using the scm_resume_thread call
 */
void scm_block_thread(void);
void scm_resume_thread(void);

/*
 * scm_collect may be called at any appropriate time in the program. It
 * processes all expired descriptors of the calling thread and frees objects
 * if their descriptor counter becomes zero.
 */
void scm_collect(void);

#endif	/* _STM_H */
