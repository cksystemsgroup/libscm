/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#include "scm.h"

#ifdef SCM_PRINTMEM
#include <malloc.h>
#endif //SCM_PRINTMEM

static long global_time = 0;
static unsigned int number_of_threads = 0;

//the number of threads, that have not yet ticked in a global period
static unsigned int ticked_threads_countdown = 1;

//protects global_time, number_of_threads and ticked_threads_countdown
static pthread_mutex_t global_time_lock = PTHREAD_MUTEX_INITIALIZER;

static descriptor_root_t *terminated_descriptor_roots = NULL;

//protects the data structures of terminated threads
static pthread_mutex_t terminated_descriptor_roots_lock = PTHREAD_MUTEX_INITIALIZER;

//thread-local storage
__thread descriptor_root_t* descriptor_root;

static inline void lock_global_time();
static inline void unlock_global_time();
static inline void lock_descriptor_roots();
static inline void unlock_descriptor_roots();

static descriptor_root_t *new_descriptor_root();
static descriptor_root_t *scm_register_thread();

static void increment_and_expire_clock(const unsigned long clock);

void *__wrap_malloc(size_t size);
void *__wrap_calloc(size_t nelem, size_t elsize);
void *__wrap_realloc(void *ptr, size_t size);
void __wrap_free(void *ptr);
size_t __wrap_malloc_usable_size(void *ptr);

//avoid ELF interposition of exported but internally used symbols
//by creating weak, hidden aliases
extern __typeof__(scm_resume_thread) scm_resume_thread_internal
    __attribute__((weak, alias("scm_resume_thread"), visibility ("hidden")));

extern __typeof__(scm_block_thread) scm_block_thread_internal
    __attribute__((weak, alias("scm_block_thread"), visibility("hidden")));

extern __typeof__(scm_collect) scm_collect_internal
    __attribute__((weak, alias("scm_collect"), visibility("hidden")));

#ifndef SCM_EAGER_COLLECTION
/**
 * Collects descriptors incrementally
 */
static void scm_lazy_collect(void);
#else
/**
 * Collects descriptors all at once
 */
static void scm_eager_collect(void);
#endif

extern __typeof__(__wrap_malloc) __wrap_malloc_internal
    __attribute__((weak, alias("__wrap_malloc"), visibility("hidden")));

/**
 * Allocates memory, e.g. with ptmalloc2, and
 * wraps object header around requested memory.
 */
void *__wrap_malloc(size_t size) {

    object_header_t *object =
        (object_header_t*) (__real_malloc(size + sizeof (object_header_t)));

    if (!object) {
        fprintf(stderr, "malloc failed.\n");
        return NULL;
    }

    object->dc_or_region_id = 0;
    object->finalizer_index = -1;

#ifdef SCM_PRINTOVERHEAD
    inc_overhead(sizeof (object_header_t));
#endif

#ifdef SCM_PRINTMEM
    inc_allocated_mem(__real_malloc_usable_size(object));
    print_memory_consumption();
#endif

    return PAYLOAD_OFFSET(object);
}

inline void *scm_malloc(size_t size) {
    return __wrap_malloc(size);
}

void *__wrap_calloc(size_t nelem, size_t elsize) {

    void *p = __wrap_malloc_internal(nelem * elsize);
    //calloc returns zeroed memory by specification
    memset(p, '\0', nelem * elsize);
    return p;
}

/**
 * Reallocates memory, e.g. with ptmalloc2, and
 * wraps object header around requested memory.
 */
void *__wrap_realloc(void *ptr, size_t size) {

    if (ptr == NULL) return __wrap_malloc_internal(size);
    //else: create new object
    object_header_t *new_object =
        (object_header_t*) __real_malloc(size + sizeof (object_header_t));

    if (!new_object) {
        fprintf(stderr, "realloc failed.\n");
        return NULL;
    }
    new_object->dc_or_region_id = 0;
    new_object->finalizer_index = -1;

#ifdef SCM_PRINTOVERHEAD
    inc_overhead(sizeof(object_header_t));
#endif

    //get the minimum of the old size and the new size
    size_t old_object_size =
        __real_malloc_usable_size(OBJECT_HEADER(ptr))
        - sizeof (object_header_t);
    size_t lesser_object_size;

    if (old_object_size >= size) {
        lesser_object_size = size;
    } else {
        lesser_object_size = old_object_size;
    }

    object_header_t *old_object = OBJECT_HEADER(ptr);
    //copy payload bytes 0..(lesser_size-1) from the old object to the new one
    memcpy(PAYLOAD_OFFSET(new_object),
           PAYLOAD_OFFSET(old_object),
           lesser_object_size);

    if (old_object->dc_or_region_id == 0) {
        //if the old object has no descriptors, we can free it
#ifdef SCM_PRINTMEM
        inc_freed_mem(__real_malloc_usable_size(old_object));
#endif

#ifdef SCM_PRINTOVERHEAD
        dec_overhead(sizeof(object_header_t));
#endif

        __real_free(old_object);
    } //else: the old object will be freed later due to expiration

#ifdef SCM_PRINTMEM
    inc_allocated_mem(__real_malloc_usable_size(new_object));
    print_memory_consumption();
#endif

    return PAYLOAD_OFFSET(new_object);
}

/**
 * Dellocates memory if object descriptor counter is 0.
 */
void __wrap_free(void *ptr) {

    if (ptr == NULL) return;

    object_header_t *object = OBJECT_HEADER(ptr);

    if (object->dc_or_region_id == 0) {
#ifdef SCM_PRINTOVERHEAD
        dec_overhead(sizeof(object_header_t));
#endif
#ifdef SCM_PRINTMEM
        inc_freed_mem(__real_malloc_usable_size(object));
#endif

        __real_free(object);
    } else {
#ifdef SCM_DEBUG
        if(object->dc_or_region_id > 0) {
            printf("Cannot free objects which are still referenced.\n");
        } else if(object->dc_or_region_id < 0) {
            printf("Cannot free single objects from a region\n");
        }
#endif
    }
}

inline void scm_free(void *ptr) {
    __wrap_free(ptr);
}

/**
 * Returns malloc usable size and
 * considers object header.
 */
size_t __wrap_malloc_usable_size(void *ptr) {

    object_header_t *object = OBJECT_HEADER(ptr);

    return __real_malloc_usable_size(object) - sizeof (object_header_t);
}

/**
 * scm_tick advances the local time of the calling thread
 */
inline void scm_tick(void) {
    scm_tick_clock(0);
}

/**
 * scm_global_tick advances the global time of the calling thread
 */
void scm_global_tick(void) {
    MICROBENCHMARK_START

#ifdef SCM_DEBUG
    printf("scm_global_tick GT: %lu GP: %lu #T:%d ttc:%d\n",
           global_time, descriptor_root->global_phase,
           number_of_threads, ticked_threads_countdown);
#endif

    if (global_time == descriptor_root->global_phase) {

        //each thread must expire its own globally clocked buffer,
        //but can only do so on its first tick after the last global
        //time advance

        //my first tick in this global period
        descriptor_root->global_phase++;

        //current_index is equal to the so-called thread-global time
        increment_current_index(&descriptor_root->globally_clocked_obj_buffer);
        increment_current_index(&descriptor_root->globally_clocked_reg_buffer);

        //expire_buffer operates on current_index - 1, so it is called after
        //we incremented the current_index of the globally_clocked_buffer
        expire_buffer(&descriptor_root->globally_clocked_obj_buffer,
                      &descriptor_root->list_of_expired_obj_descriptors);
        expire_buffer(&descriptor_root->globally_clocked_reg_buffer,
                      &descriptor_root->list_of_expired_reg_descriptors);

        if (atomic_int_dec_and_test((int*) &ticked_threads_countdown)) {
            // we are the last thread to tick in this global phase
            
            lock_global_time();

            ticked_threads_countdown = number_of_threads;
            
            //assert: descriptor_root->global_phase == global_time + 1
            global_time++;

            unlock_global_time();

        } //else global_time does not advance, other threads have to do a global_tick

    } //else we already ticked in this global_phase


	if (SCM_MAX_CLOCKS > 1) {

		unsigned int rr_index = descriptor_root->round_robin;

#ifdef SCM_CHECK_CONDITIONS
		if (rr_index == 0 || rr_index >= SCM_MAX_CLOCKS) {
			fprintf(stderr, "The round robin index = %u must never be 0 or >= SCM_MAX_CLOCKS.\n", rr_index);
			return;
		}
#endif
		unsigned int age_of_rr_buffer =
			descriptor_root->locally_clocked_obj_buffer[rr_index].age;

		// if the next round_robin buffer is a zombie -> cleanup incrementally
		if (age_of_rr_buffer != descriptor_root->current_time &&
				descriptor_root->locally_clocked_obj_buffer[rr_index]
				.not_expired_length != 0) {

			increment_and_expire_clock(rr_index);

			rr_index = (rr_index + 1) % SCM_MAX_CLOCKS;
			
            if (rr_index == 0) {
				rr_index = 1;
			}
			
            descriptor_root->round_robin = rr_index;
		}
	}

#ifndef SCM_EAGER_COLLECTION
    scm_lazy_collect();
#else
    scm_eager_collect();
#endif

#ifdef SCM_PRINTMEM
    print_memory_consumption();
#endif
    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_global_tick")
}

#ifndef SCM_EAGER_COLLECTION
/**
 * Collects descriptors incrementally
 */
void scm_lazy_collect(void) {
    expire_obj_descriptor_if_exists(&descriptor_root->list_of_expired_obj_descriptors);

    expire_reg_descriptor_if_exists(&descriptor_root->list_of_expired_reg_descriptors);
}
#else
/**
 * Collects descriptors all at once
 */
void scm_eager_collect(void) {

    while (expire_obj_descriptor_if_exists(
                &descriptor_root->list_of_expired_obj_descriptors));
    while (expire_reg_descriptor_if_exists(
                &descriptor_root->list_of_expired_reg_descriptors));
}
#endif

/**
 * Checks whether the given extension time is in the bounds of the allowed
 * extension time.
 */
static inline unsigned int check_extension(unsigned int given_extension) {
    if (given_extension > SCM_MAX_EXPIRATION_EXTENSION) {
#ifdef SCM_DEBUG
        printf("violation of SCM_MAX_EXPIRATION_EXTENT\n");
#endif

        return SCM_MAX_EXPIRATION_EXTENSION;
    } else {
        return given_extension;
    }
}

/**
 * scm_refresh() is the same as scm_global_refresh without the
 * additional extension to accommodate other threads.
 * In a multi-clock environment, scm_refresh refreshes
 * the object with the thread-local base clock.
 * If the object is part of a region, the region is refreshed instead.
 */
void scm_refresh(void *ptr, unsigned int extension) {
    scm_refresh_with_clock(ptr, extension, 0);
}

/**
 * scm_register_thread() is called on a thread when it operates the first time
 * in libscm. The thread data structures are created or reused from previously
 * terminated threads.
 */
static descriptor_root_t *scm_register_thread() {
    lock_descriptor_roots();

    if (terminated_descriptor_roots != NULL) {
        descriptor_root = terminated_descriptor_roots;
        terminated_descriptor_roots = terminated_descriptor_roots->next;
    } else {
        descriptor_root = new_descriptor_root();

#ifdef SCM_CHECK_CONDITIONS
        if(descriptor_root->round_robin != 1) {
        	fprintf(stderr, "Descriptor root initialization failed. "
        			"Round robin is %u \n", descriptor_root->round_robin);
        	exit(-1);
        }
#endif
    }

    // The current_time distinguishes from zombie descriptor
    // buffers which have another "age".
    // There will be an automatic buffer overflow if the last current_time
    // was equal to UINT_MAX.
    descriptor_root->current_time++;

    int current_time = descriptor_root->current_time;

    descriptor_root->locally_clocked_obj_buffer[0].age = current_time;
    descriptor_root->locally_clocked_reg_buffer[0].age = current_time;
    
    unlock_descriptor_roots();

    //TODO: check this
    //assert: if descriptor_root belonged to a terminated thread,
    //block_thread was invoked on this thread
    scm_resume_thread_internal();

    return descriptor_root;
}

/**
 * scm_unregister_thread() is called upon termination of a thread. The thread
 * leaves the system and passes its data structures in a pool to be reused
 * by other threads upon creation.
 */
void scm_unregister_thread() {

    scm_block_thread_internal();

    lock_descriptor_roots();

    descriptor_root->next = terminated_descriptor_roots;
    terminated_descriptor_roots = descriptor_root;

    unlock_descriptor_roots();
}

/**
 * scm_block_thread() is called when a thread blocks to notify the system about it
 */
void scm_block_thread() {

    //assert: we do not have the descriptor_roots lock
    lock_global_time();
    number_of_threads--;
    
    if(descriptor_root == NULL) {
        descriptor_root = get_descriptor_root();
    }

    //decrement ticked_threads_countdown so other threads do not have to wait
    if (global_time == descriptor_root->global_phase) {
        //we have not ticked in this global period
        if (atomic_int_dec_and_test((int*) &ticked_threads_countdown)) {
            //we are the last thread to tick and therefore need to tick globally
            if (number_of_threads == 0) {
                ticked_threads_countdown = 1;
            } else {
                ticked_threads_countdown = number_of_threads;
            }

            global_time++;
        } else {
            //there are other threads to tick before global time advances
        }
    } else {
        //we have already ticked globally in this global phase.
    }

    unlock_global_time();
}

/**
 * scm_resume_thread() is called when a thread returns from blocking state to
 * notify the system about it.
 */
void scm_resume_thread() {

    //assert: we do not have the descriptor_roots lock
    lock_global_time();

    if (number_of_threads == 0) {
        /* if this is the first thread to resume/register,
         * then we have to tick to make
         * global progress, unless another thread registers
         * assert: ticked_threads_countdown == 1
         */
        descriptor_root->global_phase = global_time;
    } else {
        //else: we do not tick globally in the current global period
        //to avoid decrement of the ticked_threads_countdown
        descriptor_root->global_phase = global_time + 1;
    }

    number_of_threads++;

    unlock_global_time();
}

/**
 * new_descriptor_root() allocates space for the descriptor_root and
 * initializes its data.
 */
static descriptor_root_t* new_descriptor_root() {

    //allocate descriptor_root 0 initialized
    descriptor_root_t *descriptor_root =
        __real_calloc(1, sizeof(descriptor_root_t));

    memset(descriptor_root, 0, sizeof(descriptor_root_t));

#ifdef SCM_PRINTOVERHEAD
    inc_overhead(__real_malloc_usable_size(descriptor_root));
#endif
#ifdef SCM_PRINTMEM
    inc_allocated_mem(__real_malloc_usable_size(descriptor_root));
#endif

    descriptor_root->globally_clocked_obj_buffer.not_expired_length =
        SCM_MAX_EXPIRATION_EXTENSION + 2;
    descriptor_root->globally_clocked_reg_buffer.not_expired_length =
        SCM_MAX_EXPIRATION_EXTENSION + 2;
    descriptor_root->locally_clocked_obj_buffer[0].not_expired_length =
        SCM_MAX_EXPIRATION_EXTENSION + 1;
    descriptor_root->locally_clocked_reg_buffer[0].not_expired_length =
        SCM_MAX_EXPIRATION_EXTENSION + 1;

    descriptor_root->round_robin = 1;
    descriptor_root->next_clock_index = 1;

    return descriptor_root;
}

/**
 * lock_global_time() uses a pthread mutex to lock the global time variable.
 */
static inline void lock_global_time() {
#ifdef SCM_PRINTLOCK
    if (pthread_mutex_trylock(&global_time_lock)) {
        printf("thread %ld BLOCKS on global_time_lock\n", pthread_self());
        pthread_mutex_lock(&global_time_lock);
    }
#else
    pthread_mutex_lock(&global_time_lock);
#endif
}

/**
 * unlock_global_time() releases the mutex for the global time variable.
 */
static inline void unlock_global_time() {
    pthread_mutex_unlock(&global_time_lock);
}

/**
 * lock_descriptor_roots() locks the descriptor roots.
 */
static inline void lock_descriptor_roots() {
#ifdef SCM_PRINTLOCK
    if (pthread_mutex_trylock(&terminated_descriptor_roots_lock)) {
        printf("thread %ld BLOCKS on terminated_descriptor_roots_lock\n", pthread_self());
        pthread_mutex_lock(&terminated_descriptor_roots_lock);
    }
#else
    pthread_mutex_lock(&terminated_descriptor_roots_lock);
#endif
}

/**
 * unlock_descriptor_roots() releases the lock of the descriptor roots.
 */
static inline void unlock_descriptor_roots() {
    pthread_mutex_unlock(&terminated_descriptor_roots_lock);
}

/**
 * scm_register_clock() returns a const integer representing
 * a new clock in the short-term memory model.
 * A clock identifies a descriptor buffer in the array of locally
 * clocked descriptor buffers of the descriptor root.
 * If all available clocks/descriptor buffers are in use, the return value is
 * set to -1, indicating an error for the caller function.
 */
const int scm_register_clock() {

    if(descriptor_root == NULL) {
        descriptor_root = get_descriptor_root();
    }
    if(SCM_MAX_CLOCKS <= 1) {
        fprintf(stderr, "libscm was build without multiclock support. "
                "Set SCM_MAX_CLOCKS to > 1 if you want to use multiple clocks.\n");
        exit(-1);
    }
    int start_index = descriptor_root->next_clock_index;
    int i = start_index;
    
    while (descriptor_root->locally_clocked_obj_buffer[i].age ==
            descriptor_root->current_time) {
        i = (i+1) % SCM_MAX_CLOCKS;
        i = i != 0 ? i : 1;
        if (i == start_index) {
            fprintf(stderr, "Clock contingency exceeded.\n");
            exit(-1);
        }
    }
    start_index = (i+1) % SCM_MAX_CLOCKS;
    start_index = start_index != 0 ? start_index : 1;
    descriptor_root->next_clock_index = start_index;
    descriptor_root->locally_clocked_obj_buffer[i].not_expired_length =
        SCM_MAX_EXPIRATION_EXTENSION + 1;
    descriptor_root->locally_clocked_reg_buffer[i].not_expired_length =
        SCM_MAX_EXPIRATION_EXTENSION + 1;

    descriptor_root->locally_clocked_obj_buffer[i].age =
        descriptor_root->current_time;
    descriptor_root->locally_clocked_reg_buffer[i].age =
        descriptor_root->current_time;

    return (const int) i;
}

/**
 * scm_unregister_clock() sets the age of the descriptor buffer
 * back to a value that is not equal to the descriptor_root current_time. 
 * As a consequence the clock buffer
 * will be cleaned up incrementally during scm_tick() calls.
 */
void scm_unregister_clock(const int clock) {

#ifdef SCM_CHECK_CONDITIONS
    if (clock <= 1 || clock >= SCM_MAX_CLOCKS) {
        fprintf(stderr, "Clock index is invalid.\n");
        return;
    }
#endif

    descriptor_root->locally_clocked_obj_buffer[clock].age =
        (descriptor_root->current_time - 1);
    descriptor_root->locally_clocked_reg_buffer[clock].age =
        (descriptor_root->current_time - 1);
}

/**
 * increment_and_expire() increments the current index of
 * the locally clocked descriptor buffers
 * and expires the descriptors from the last index
 */
void increment_and_expire_clock(const unsigned long clock) {

#ifdef SCM_CHECK_CONDITIONS
    if (clock < 0 || clock >= SCM_MAX_CLOCKS) {
        fprintf(stderr, "The given clock is out of range.\n");
        exit(-1);
    }
#endif
    //make local time progress
    //current_index is equal to the so-called thread-local time
    increment_current_index(
        &descriptor_root->locally_clocked_obj_buffer[clock]);
    increment_current_index(
        &descriptor_root->locally_clocked_reg_buffer[clock]);

    //expire_buffer operates on current_index - 1, so it is called after
    //we incremented the current_index of the locally_clocked_buffer
    expire_buffer(&descriptor_root->locally_clocked_obj_buffer[clock],
                  &descriptor_root->list_of_expired_obj_descriptors);
    expire_buffer(&descriptor_root->locally_clocked_reg_buffer[clock],
                  &descriptor_root->list_of_expired_reg_descriptors);
}

/**
 * scm_tick_clock() is used to advance the time of the 
 * given thread-local clock
 */
void scm_tick_clock(const unsigned long clock) {
    MICROBENCHMARK_START

#ifdef SCM_DEBUG
    printf("Ticking clock: %u\n", clock);
#endif
    increment_and_expire_clock(clock);


	if(SCM_MAX_CLOCKS > 1) {
		unsigned int rr_index = descriptor_root->round_robin;
		if (rr_index == clock) {
			rr_index = (rr_index + 1) % SCM_MAX_CLOCKS;
			if (rr_index == 0) {
				rr_index = 1;
			}
		}


#ifdef SCM_CHECK_CONDITIONS
		if (rr_index == 0 || rr_index >= SCM_MAX_CLOCKS) {
			fprintf(stderr, "The round robin index is %u\n", rr_index);
			exit(-1);
		}
#endif

		unsigned int age_of_rr_buffer =
			descriptor_root->locally_clocked_obj_buffer[rr_index].age;

		// if the next round_robin buffer is a zombie -> cleanup incrementally
		if ( age_of_rr_buffer != descriptor_root->current_time &&
            descriptor_root->locally_clocked_obj_buffer[rr_index]
				.not_expired_length != 0) {

			increment_and_expire_clock(rr_index);

			rr_index = (rr_index + 1) % SCM_MAX_CLOCKS;
			if (rr_index == 0) {
				rr_index = 1;
			}
			descriptor_root->round_robin = rr_index;

		}
	}
#ifndef SCM_EAGER_COLLECTION

    //we also process expired descriptors at tick
    //to get a cyclic allocation/free scheme. this is optional
    scm_lazy_collect();
#else
    scm_eager_collect();
#endif

#ifdef SCM_PRINTMEM
    print_memory_consumption();
#endif

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_tick")
}

/**
 * scm_refresh_with_clock() refreshes a given object with a given clock,
 * which can be different to the thread-local base clock.
 * If an object is refreshed with multiple clocks it lives
 * until all clocks ticked n times, where n is the respective extension.
 * If the object is part of a region, the region is refreshed instead.
 */
void scm_refresh_with_clock(void *ptr, unsigned int extension, const unsigned long clock) {
    MICROBENCHMARK_START

    if(descriptor_root == NULL) {
		descriptor_root = get_descriptor_root();
	}

    object_header_t *object = OBJECT_HEADER(ptr);

    // is the object allocated into a region?
    if (object->dc_or_region_id < 0) {
        int region_id = object->dc_or_region_id & ~HB_MASK;
        scm_refresh_region_with_clock(region_id, extension, clock);
    } else {
    	extension = check_extension(extension);

#ifdef SCM_DEBUG
    printf("scm_refresh(%lx, %d)\n", (unsigned long) ptr, extension);
#endif

// check pre-conditions
#ifdef SCM_CHECK_CONDITIONS
        if (ptr == NULL) {
            fprintf(stderr, "Cannot refresh NULL pointer\n");
            return;
        }
        if (descriptor_root->current_time !=
                descriptor_root->locally_clocked_reg_buffer[clock].age ||
                descriptor_root->locally_clocked_reg_buffer[clock]
                .not_expired_length == 0) {
            fprintf(stderr, "Cannot refresh zombie clock");
            return;
        }
#endif
        if (object->dc_or_region_id == INT_MAX) {
            fprintf(stderr, "Descriptor counter reached max value");
            return;
        }
        atomic_int_inc((int*) & object->dc_or_region_id);
        insert_descriptor(object,
                          &descriptor_root->locally_clocked_obj_buffer[clock], extension);
    }

#ifndef SCM_EAGER_COLLECTION
    scm_lazy_collect();
#else
    //do nothing. expired descriptors are collected at tick
#endif

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_refresh")
}