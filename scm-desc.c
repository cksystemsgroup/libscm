/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "stm.h"
#include "scm-desc.h"
#include "stm-debug.h"
#include "meter.h"
#include "arch.h"

#ifdef SCM_CALCMEM
#include <malloc.h>
#endif //SCM_CALCMEM

#ifdef SCM_MAKE_MICROBENCHMARKS
//declare a thread-local field to measure the overhead by malloc and free
__thread unsigned long long allocator_overhead __attribute__((aligned (64))) = 0;
#endif

static long global_time = 0;
static unsigned int number_of_threads = 0;

//the number of threads, that have not yet ticked in a global period
static unsigned int ticked_threads_countdown = 1;

//protects global_time, number_of_threads and ticked_threads_countdown
static pthread_mutex_t global_time_lock = PTHREAD_MUTEX_INITIALIZER;

static descriptor_root_t *terminated_descriptor_roots = NULL;

//protects the data structures of terminated threads
static pthread_mutex_t terminated_descriptor_roots_lock =
        PTHREAD_MUTEX_INITIALIZER;

//thread local reference to the descriptor root
__thread descriptor_root_t *descriptor_root;

static inline void lock_global_time();
static inline void unlock_global_time();
static inline void lock_descriptor_roots();
static inline void unlock_descriptor_roots();
static descriptor_root_t *new_descriptor_root();

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

extern __typeof__(__wrap_malloc) __wrap_malloc_internal
    __attribute__((weak, alias("__wrap_malloc"), visibility("hidden")));

void *__wrap_malloc(size_t size) {
    MICROBENCHMARK_START
            
    OVERHEAD_START
    object_header_t *object =
            (object_header_t*) (__real_malloc(size + sizeof (object_header_t)));
    OVERHEAD_STOP

    if (!object) {
        perror("malloc");
        MICROBENCHMARK_STOP
        MICROBENCHMARK_DURATION("malloc")
        return NULL;
    }
    object->dc = 0;
    object->finalizer_index = -1;

#ifdef SCM_CALCOVERHEAD
    inc_overhead(sizeof (object_header_t));
#endif

#ifdef SCM_CALCMEM
    inc_allocated_mem(__real_malloc_usable_size(object));
#ifdef SCM_PRINTMEM
    print_memory_consumption();
#endif
#endif

    void *ptr = PAYLOAD_OFFSET(object);

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("malloc")

    return ptr;
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

void *__wrap_realloc(void *ptr, size_t size) {
    MICROBENCHMARK_START

    if (ptr == NULL) return __wrap_malloc_internal(size);
    //else: create new object
    OVERHEAD_START
    object_header_t *new_object =
            (object_header_t*) __real_malloc(size + sizeof (object_header_t));
    OVERHEAD_STOP

    if (!new_object) {
        perror("realloc");
        MICROBENCHMARK_STOP
        MICROBENCHMARK_DURATION("realloc")
        return NULL;
    }
    new_object->dc = 0;
    new_object->finalizer_index = -1;

#ifdef SCM_CALCOVERHEAD
    inc_overhead(sizeof (object_header_t));
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

    if (old_object->dc == 0) {
        //if the old object has no descriptors, we can free it
#ifdef SCM_CALCMEM
        inc_freed_mem(__real_malloc_usable_size(old_object));
#endif

#ifdef SCM_CALCOVERHEAD
        dec_overhead(sizeof (object_header_t));
#endif
        OVERHEAD_START
        __real_free(old_object);
        OVERHEAD_STOP
    } //else: the old object will be freed later due to expiration

#ifdef SCM_CALCMEM
    inc_allocated_mem(__real_malloc_usable_size(new_object));
#ifdef SCM_PRINTMEM
    print_memory_consumption();
#endif
#endif

    void *new_ptr = PAYLOAD_OFFSET(new_object);

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("realloc")

    return new_ptr;
}

void __wrap_free(void *ptr) {
    MICROBENCHMARK_START

    if (ptr == NULL) {
        MICROBENCHMARK_STOP
        MICROBENCHMARK_DURATION("free")
        return;
    }

    object_header_t *object = OBJECT_HEADER(ptr);

    if (object->dc == 0) {
#ifdef SCM_CALCOVERHEAD
        dec_overhead(sizeof (object_header_t));
#endif
#ifdef SCM_CALCMEM
        inc_freed_mem(__real_malloc_usable_size(object));
#endif
        OVERHEAD_START
        __real_free(object);
        OVERHEAD_STOP
    }

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("free")
}

inline void scm_free(void *ptr) {
    __wrap_free(ptr);
}

size_t __wrap_malloc_usable_size(void *ptr) {
    MICROBENCHMARK_START

    object_header_t *object = OBJECT_HEADER(ptr);
    size_t sz = __real_malloc_usable_size(object) - sizeof (object_header_t);

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("malloc_usable_size")
    return sz;
}

void scm_tick(void) {
    MICROBENCHMARK_START

    descriptor_root_t *dr = get_descriptor_root();

    //make local time progress
    //current_index is equal to the so-called thread-local time
    increment_current_index(&dr->locally_clocked_buffer);

    //expire_buffer operates on current_index - 1, so it is called after
    //we incremented the current_index of the locally_clocked_buffer
    expire_buffer(&dr->locally_clocked_buffer,
            &dr->list_of_expired_descriptors);

#ifndef SCM_EAGER_COLLECTION
    //we also process expired descriptors at tick
    //to get a cyclic allocation/free scheme. this is optional
    expire_descriptor_if_exists(&dr->list_of_expired_descriptors);
#else
    scm_collect_internal();
#endif

#ifdef SCM_PRINTMEM
    print_memory_consumption();
#endif

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_tick")
}

void scm_global_tick(void) {
    MICROBENCHMARK_START
    descriptor_root_t *dr = get_descriptor_root();

#ifdef SCM_DEBUG
    printf("scm_global_tick GT: %lu GP: %lu #T:%d ttc:%d\n",
            global_time, descriptor_root->global_phase,
            number_of_threads, ticked_threads_countdown);
#endif

    if (global_time == dr->global_phase) {
        //each thread must expire its own globally clocked buffer,
        //but can only do so on its first tick after the last global
        //time advance

        //my first tick in this global period
        dr->global_phase++;

        //current_index is equal to the so-called thread-global time
        increment_current_index(&dr->globally_clocked_buffer);

        //expire_buffer operates on current_index - 1, so it is called after
        //we incremented the current_index of the globally_clocked_buffer
        expire_buffer(&dr->globally_clocked_buffer,
                &dr->list_of_expired_descriptors);

        if (atomic_int_dec_and_test((int*) & ticked_threads_countdown)) {
            // we are the last thread to tick in this global phase

            lock_global_time();

            //reset the ticked_threads_countdown
            ticked_threads_countdown = number_of_threads;

            //assert: descriptor_root->global_phase == global_time + 1
            global_time++;
            //printf("GLOBAL TIME ADVANCE %lu\n", global_time);

            unlock_global_time();

        } //else global time does not advance, other threads have to do a 
        //global_tick

    } //else: we already ticked in this global_phase
    // each thread can only do a global_tick once per global phase

#ifndef SCM_EAGER_COLLECTION
    //we also process expired descriptors at tick
    //to get a cyclic allocation/free scheme. this is optional
    expire_descriptor_if_exists(&dr->list_of_expired_descriptors);
#else
    scm_collect_internal();
#endif

#ifdef SCM_PRINTMEM
    print_memory_consumption();
#endif
    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_global_tick")
}

void scm_collect(void) {
    //MICROBENCHMARK_START

    descriptor_root_t *dr = get_descriptor_root();

    while (expire_descriptor_if_exists(
            &dr->list_of_expired_descriptors));

    //MICROBENCHMARK_STOP
    //MICROBENCHMARK_DURATION("scm_collect")
}

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

void scm_global_refresh(void *ptr, unsigned int extension) {
    MICROBENCHMARK_START
#ifdef SCM_DEBUG
            printf("scm_global_refresh(%lx, %d)\n", (unsigned long) ptr, extension);
#endif

    descriptor_root_t *dr = get_descriptor_root();
    object_header_t *object = OBJECT_HEADER(ptr);

    extension = check_extension(extension);

    atomic_int_inc((int*) & object->dc);
    insert_descriptor(object,
            &dr->globally_clocked_buffer, extension + 2);

#ifndef SCM_EAGER_COLLECTION
    expire_descriptor_if_exists(&dr->list_of_expired_descriptors);
#else
    //do nothing. expired descriptors were already collected at last tick
#endif

#ifdef SCM_PRINTMEM
    print_memory_consumption();
#endif
    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_global_refresh")
}

void scm_refresh(void *ptr, unsigned int extension) {
    MICROBENCHMARK_START
#ifdef SCM_DEBUG
            printf("scm_refresh(%lx, %d)\n", (unsigned long) ptr, extension);
#endif

    descriptor_root_t *dr = get_descriptor_root();
    object_header_t *object = OBJECT_HEADER(ptr);

    extension = check_extension(extension);

    atomic_int_inc((int*) & object->dc);
    insert_descriptor(object,
            &dr->locally_clocked_buffer, extension);

#ifndef SCM_EAGER_COLLECTION
    expire_descriptor_if_exists(&dr->list_of_expired_descriptors);
#else
    //do nothing. expired descriptors were already collected at last tick
#endif

#ifdef SCM_PRINTMEM
    print_memory_consumption();
#endif
    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_refresh")
}

/*
 * get_descriptor_root is called by operations that need to access the thread
 * local meta data structures.
 */
inline descriptor_root_t *get_descriptor_root() {
    //void *ptr = pthread_getspecific(descriptor_root_key);
    //return (descriptor_root_t*) ptr;
    return descriptor_root;
}

/*
 * scm_register_thread is called on a thread when it operates the first time
 * in libscm. The thread data structures are created or reused from previously
 * terminated threads.
 */
void scm_register_thread() {
    lock_descriptor_roots();

    if (terminated_descriptor_roots != NULL) {
        descriptor_root = terminated_descriptor_roots;

        terminated_descriptor_roots = terminated_descriptor_roots->next;
    } else {
        descriptor_root = new_descriptor_root();
    }

    unlock_descriptor_roots();

    //assert: if descriptor_root belonged to a terminated thread,
    //block_thread was invoked on this thread
    scm_resume_thread_internal();
}

/*
 * scm_unregister_thread is called upon termination of a thread. The thread 
 * leaves the system and passes its data structures in a pool to be reused
 * by other threads upon creation.
 */
void scm_unregister_thread() {
    MICROBENCHMARK_START

    scm_block_thread_internal();

    descriptor_root_t *dr = get_descriptor_root();

    lock_descriptor_roots();

    dr->next = terminated_descriptor_roots;
    terminated_descriptor_roots = dr;

    unlock_descriptor_roots();

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_unregister_thread")
}

/*
 * scm_block_thread is called when a thread blocks to notify the system about it
 */
void scm_block_thread() {
    MICROBENCHMARK_START

    descriptor_root_t *dr = get_descriptor_root();

    //assert: we do not have the descriptor_roots lock
    lock_global_time();
    number_of_threads--;

    if (global_time == dr->global_phase) {
        //we have not ticked in this global period

        //decrement ticked_threads_countdown so other threads don't have to wait
        if (atomic_int_dec_and_test((int*) & ticked_threads_countdown)) {
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

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_block_thread")
}

/*
 * scm_resume_thread is called when a thread returns from blocking state to 
 * notify the system about it.
 */
void scm_resume_thread() {
    MICROBENCHMARK_START

    descriptor_root_t *dr = get_descriptor_root();

    //assert: we do not have the descriptor_roots lock
    lock_global_time();

    if (number_of_threads == 0) {
        /* if this is the first thread to resume/register,
         * then we have to tick to make
         * global progress, unless another thread registers
         * assert: ticked_threads_countdown == 1
         */
        dr->global_phase = global_time;
    } else {
        //else: we do not tick globally in the current global period
        //to avoid decrement of the ticked_threads_countdown
        dr->global_phase = global_time + 1;
    }
    number_of_threads++;

    unlock_global_time();

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_resume_thread")
}

static descriptor_root_t *new_descriptor_root() {

    //allocate descriptor_root 0 initialized
    OVERHEAD_START
    descriptor_root_t *descriptor_root =
            __real_calloc(1, sizeof (descriptor_root_t));
    OVERHEAD_STOP

#ifdef SCM_CALCOVERHEAD
    inc_overhead(__real_malloc_usable_size(descriptor_root));
#endif
#ifdef SCM_CALCMEM
    //inc_allocated_mem(__real_malloc_usable_size(descriptor_root));
#endif

    descriptor_root->globally_clocked_buffer.not_expired_length =
            SCM_MAX_EXPIRATION_EXTENSION + 3;
    descriptor_root->locally_clocked_buffer.not_expired_length =
            SCM_MAX_EXPIRATION_EXTENSION + 1;

    return descriptor_root;
}

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

static inline void unlock_global_time() {
    pthread_mutex_unlock(&global_time_lock);
}

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

static inline void unlock_descriptor_roots() {
    pthread_mutex_unlock(&terminated_descriptor_roots_lock);
}
