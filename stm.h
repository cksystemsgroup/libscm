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

#ifndef _STM_H
#define	_STM_H

#include <stddef.h>

/*
 * one may use the following compile time configuration for libscm.
 * See Makefile for different configurations
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
 * print bookkeeping memory oberhead.
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
 * #define SCM_DECRIPTOR_PAGE_FREELIST_SIZE 10
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

#ifndef SCM_DECRIPTOR_PAGE_FREELIST_SIZE
#define SCM_DECRIPTOR_PAGE_FREELIST_SIZE 10
#endif

/*
 * scm_malloc is used to allocate short term memory objects. This function
 * can be used at compile time. Unmodified code which uses e.g. glibc's
 * malloc can be used with linker option --wrap malloc
 */
void *scm_malloc(size_t size);

/*
 * scm_free is used to free short term memory objects with no descriptors on
 * them e.g. permanent objects. This function can be used at compile time.
 * Unmodified code which uses e.g. glibc's free can be used with linker
 * option --wrap free
 */
void scm_free(void *ptr);

/*
 * scm_global_tick signals that the calling thread is ready to have the global
 * time increased
 */
void scm_global_tick(void);

/*
 * scm_tick is used to advance the local time of the calling thread
 */
void scm_tick(void);

/*
 * scm_global_refresh adds extension time units to the expiration time of
 * ptr and takes care that all other threads have enough time to also call
 * global_refresh(ptr, extension)
 */
void scm_global_refresh(void *ptr, unsigned int extension);

/*
 * scm_refresh is the same as scm_global_refresh but does not take
 * care for other threads.
 */
void scm_refresh(void *ptr, unsigned int extension);

/*
 * scm_register_thread initializes self-collecting mutators for the calling 
 * thread. This function needs to be invoked before using refresh and tick
 * operations.
 */
void scm_register_thread(void);

/*
 * scm_unregister_thread may be called just before a thread terminates.
 * The thread's data structures are preserved for a new thread to join
 * the short term memory system.
 */
void scm_unregister_thread(void);

/*
 * scm_block_thread may be used to signal the short term memory system that
 * the calling thread is about to leave the system for a while e.g. because of
 * a blocking call. During this period the system does not wait for scm_tick
 * calls of this thread.
 * After the thread finished the blocking state it re-joins the short term
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
