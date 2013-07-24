/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#include "scm.h"

/**
 * Allocates memory, e.g. with ptmalloc2, and
 * wraps object header around requested memory.
 */
void *__wrap_malloc(size_t size) {

    object_header_t *object =
        (object_header_t*) (__real_malloc(size + sizeof(object_header_t)));

    if (!object) {
#ifdef SCM_DEBUG
        printf("malloc failed.\n");
#endif
        return NULL;
    }

    object->dc_or_region_id = 0;
    object->finalizer_index = -1;

#ifdef SCM_RECORD_MEMORY_USAGE
    inc_overhead(sizeof(object_header_t));
    inc_allocated_mem(__real_malloc_usable_size(object));

    print_memory_consumption();
#endif

    return PAYLOAD_OFFSET(object);
}

//avoid ELF interposition of exported but internally used symbols
//by creating weak, hidden aliases
extern __typeof__(__wrap_malloc) __wrap_malloc_internal
    __attribute__((weak, alias("__wrap_malloc"), visibility("hidden")));

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
        (object_header_t*) __real_malloc(size + sizeof(object_header_t));

    if (!new_object) {
#ifdef SCM_DEBUG
        printf("realloc failed.\n");
#endif
        return NULL;
    }
    new_object->dc_or_region_id = 0;
    new_object->finalizer_index = -1;

#ifdef SCM_RECORD_MEMORY_USAGE
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

#ifdef SCM_RECORD_MEMORY_USAGE
        inc_freed_mem(__real_malloc_usable_size(old_object));
        dec_overhead(sizeof(object_header_t));
#endif

        __real_free(old_object);
    } //else: the old object will be freed later due to expiration

#ifdef SCM_RECORD_MEMORY_USAGE
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
#ifdef SCM_RECORD_MEMORY_USAGE
        dec_overhead(sizeof(object_header_t));
        inc_freed_mem(__real_malloc_usable_size(object));
#endif

        __real_free(object);
    } else {
#ifdef SCM_DEBUG
        if(object->dc_or_region_id > 0) {
            printf("Cannot free objects which are still referenced.\n");
        } else if(object->dc_or_region_id < 0) {
            printf("Cannot free single objects from a region.\n");
        }
#endif
    }
}

extern __typeof__(__wrap_free) __wrap_free_internal
    __attribute__((weak, alias("__wrap_free"), visibility("hidden")));

/**
 * Returns malloc usable size and
 * considers object header.
 */
size_t __wrap_malloc_usable_size(void *ptr) {

    object_header_t *object = OBJECT_HEADER(ptr);

    return __real_malloc_usable_size(object) - sizeof(object_header_t);
}

// The descriptor root is stored as thread-local storage variable.
// According to perf tools from Google __thread is faster than
// pthread_getspecific().
__thread descriptor_root_t* descriptor_root __attribute__((tls_model("initial-exec")));

static descriptor_root_t *terminated_descriptor_roots = NULL;

//protects the data structures of terminated threads
static pthread_mutex_t terminated_descriptor_roots_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * lock_descriptor_roots() locks the descriptor roots.
 */
static inline void lock_descriptor_roots() {
#ifdef SCM_PRINT_BLOCKING
    if (pthread_mutex_trylock(&terminated_descriptor_roots_lock)) {
        printf("Thread %p BLOCKS on terminated_descriptor_roots_lock.\n", (void*) pthread_self());
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
 * new_descriptor_root() allocates space for the descriptor_root and
 * initializes its data.
 */
static descriptor_root_t* new_descriptor_root() {

    //allocate descriptor_root 0 initialized
    descriptor_root_t *descriptor_root =
        __real_calloc(1, sizeof(descriptor_root_t));

#ifdef SCM_RECORD_MEMORY_USAGE
    inc_overhead(__real_malloc_usable_size(descriptor_root));
    inc_allocated_mem(__real_malloc_usable_size(descriptor_root));
#endif

    descriptor_root->next_clock_index = 1;

    descriptor_root->globally_clocked_obj_buffer.not_expired_length =
        SCM_MAX_EXPIRATION_EXTENSION + 2;
    descriptor_root->globally_clocked_reg_buffer.not_expired_length =
        SCM_MAX_EXPIRATION_EXTENSION + 2;
    descriptor_root->locally_clocked_obj_buffer[0].not_expired_length =
        SCM_MAX_EXPIRATION_EXTENSION + 1;
    descriptor_root->locally_clocked_reg_buffer[0].not_expired_length =
        SCM_MAX_EXPIRATION_EXTENSION + 1;

    descriptor_root->round_robin = 1;
    descriptor_root->blocked = true;

    return descriptor_root;
}

static long global_time = 0;

//the number of threads registered for global time advance
static unsigned int number_of_threads = 0;

//the number of threads, that have not yet ticked in a global period
static unsigned int ticked_threads_countdown = 1;

//protects global_time, number_of_threads, and ticked_threads_countdown
static pthread_mutex_t global_time_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * lock_global_time() uses a pthread mutex to lock the global time variable.
 */
static inline void lock_global_time() {
#ifdef SCM_PRINT_BLOCKING
    if (pthread_mutex_trylock(&global_time_lock)) {
        printf("Thread %p BLOCKS on global_time_lock.\n", (void*) pthread_self());
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
 * scm_block_thread() should be called before a thread blocks to notify the system about it
 */
void scm_block_thread() {
    if (descriptor_root == NULL) {
        return;
    }

    if (descriptor_root->blocked) {
#ifdef SCM_DEBUG
        printf("scm_block_thread: thread is already blocked.\n");
#endif

        return;
    }

    //assert: we do not have the descriptor_roots lock
    lock_global_time();
    number_of_threads--;
    
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

    descriptor_root->blocked = true;
}

extern __typeof__(scm_block_thread) scm_block_thread_internal
    __attribute__((weak, alias("scm_block_thread"), visibility("hidden")));

/**
 * scm_resume_thread() is called when a thread returns from blocking state to
 * notify the system about it.
 */
void scm_resume_thread() {
    if (descriptor_root == NULL) {
        return;
    }

    if (!descriptor_root->blocked) {
#ifdef SCM_DEBUG
        printf("scm_resume_thread: thread is not blocked.\n");
#endif

        return;
    }

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

    descriptor_root->blocked = false;
}

extern __typeof__(scm_resume_thread) scm_resume_thread_internal
    __attribute__((weak, alias("scm_resume_thread"), visibility ("hidden")));

/**
 * register_thread() is called on a thread when it operates the first time
 * in libscm. The thread data structures are created or reused from previously
 * terminated threads.
 */
void register_thread() {
    lock_descriptor_roots();

    if (terminated_descriptor_roots != NULL) {
        descriptor_root = terminated_descriptor_roots;
        terminated_descriptor_roots = terminated_descriptor_roots->next;
    } else {
        descriptor_root = new_descriptor_root();

#ifdef SCM_CHECK_CONDITIONS
        if(descriptor_root->round_robin != 1) {
            printf("Descriptor root initialization failed. Round robin is %u.\n", descriptor_root->round_robin);
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

    //assert: if descriptor_root belonged to a terminated thread,
    //block_thread was invoked on this thread
    scm_resume_thread_internal();
}

/**
 * unregister_thread() is called upon termination of a thread. The thread
 * leaves the system and passes its data structures to a pool to be reused
 * by other threads upon their creation.
 */
static void unregister_thread(void* key_variable) {
    if (descriptor_root != NULL) {
        scm_block_thread_internal();

        lock_descriptor_roots();

        descriptor_root->next = terminated_descriptor_roots;
        terminated_descriptor_roots = descriptor_root;

        descriptor_root = NULL;

        unlock_descriptor_roots();
    }
}

static pthread_key_t descriptor_root_key;

static void make_key() {
    // create the key and set the destructor
    if (pthread_key_create(&descriptor_root_key, unregister_thread) != 0) {
#ifdef SCM_DEBUG
        printf("pthread_key_create failed.\n");
        exit(-1);
#endif
    }
}

static pthread_once_t thread_once_control = PTHREAD_ONCE_INIT;

static void create_descriptor_root() {
    if (descriptor_root != NULL) {
        return;
    }

    // create the key, once
    if (pthread_once(&thread_once_control, make_key) != 0) {
#ifdef SCM_DEBUG
        printf("pthread_once failed.\n");
        exit(-1);
#endif
    }

    register_thread();

    // IMPORTANT: also set the pthread key although we will never read from it,
    // since the destructor is only executed if the value behind the key != 0.
    if (pthread_setspecific(descriptor_root_key, (void*) descriptor_root) != 0) {
#ifdef SCM_DEBUG
        printf("pthread_setspecific failed.\n");
        exit(-1);
#endif
    }
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
    create_descriptor_root();

    if (SCM_MAX_CLOCKS <= 1) {
#ifdef SCM_DEBUG
        printf("libscm was built without multiclock support. Set SCM_MAX_CLOCKS to > 1 to use multiple clocks.\n");
#endif
        return(-1);
    }

    int start_index = descriptor_root->next_clock_index;
    int i = start_index;
    
    while (descriptor_root->locally_clocked_obj_buffer[i].age ==
            descriptor_root->current_time) {
        i = (i+1) % SCM_MAX_CLOCKS;
        i = i != 0 ? i : 1;
        if (i == start_index) {
#ifdef SCM_DEBUG
            printf("Clock contingency exceeded.\n");
#endif
            return(-1);
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
    if (descriptor_root == NULL) {
        return;
    }

    if (clock <= 1 || clock >= SCM_MAX_CLOCKS) {
#ifdef SCM_DEBUG
        printf("Clock index is invalid.\n");
#endif
        return;
    }

    descriptor_root->locally_clocked_obj_buffer[clock].age =
        (descriptor_root->current_time - 1);
    descriptor_root->locally_clocked_reg_buffer[clock].age =
        (descriptor_root->current_time - 1);
}

/**
 * init_region_page() creates and initializes a new region page if no other
 * region page exists or if all other region pages are full.
 * The region_page is allocated page-aligned.
 */
static region_page_t* init_region_page(region_t* region) {
// check pre-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region == NULL) {
        printf("Cannot initialize region page for NULL region.\n");
        exit(-1);
    } else if (region->age != descriptor_root->current_time) {
        printf("Initializing region page into zombie region is not allowed.\n");
    }
    region_t* invar_region = region;
    region_page_t* invar_first_region_page = region->firstPage;
#endif

    region_page_t* prevLastPage = region->lastPage;

    region_page_t* new_page = descriptor_root->region_page_pool;

    if (new_page != NULL) {

        descriptor_root->region_page_pool = new_page->nextPage;
        descriptor_root->number_of_pooled_region_pages--;
#ifdef SCM_RECORD_MEMORY_USAGE
        dec_pooled_mem(sizeof (region_page_t));
#endif
    }
    else {
        new_page = __real_malloc(SCM_REGION_PAGE_SIZE);

        if (new_page == NULL) {
#ifdef SCM_DEBUG
            printf("Memory for region page could not be allocated.\n");
#endif
            exit(-1);
        }

#ifdef SCM_RECORD_MEMORY_USAGE
        inc_overhead(__real_malloc_usable_size(new_page) - SCM_REGION_PAGE_PAYLOAD_SIZE);
        inc_allocated_mem(__real_malloc_usable_size(new_page));
#endif
    }

    memset(new_page, '\0', SCM_REGION_PAGE_SIZE);

    if (prevLastPage != NULL) {
        prevLastPage->nextPage = new_page;
    }

    region->last_address_in_last_page =
            &new_page->memory + SCM_REGION_PAGE_PAYLOAD_SIZE;
    region->lastPage = new_page;
    region->number_of_region_pages++;

// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region == NULL) {
        printf("The region became NULL during initialization of a region page.\n");
    } else if (region != invar_region || region->firstPage != invar_first_region_page) {
        printf("The region or the first region page changed during initialization.\n");
    } else if (new_page == NULL || new_page->nextPage != NULL) {
        printf("The new region page was not correctly initialized.\n");
    }
#endif

    return new_page;
}

/**
 * scm_create_region() returns a const integer representing a new region
 * if available and -1 otherwise. The new region is detected by scanning
 * the descriptor_root regions array for a region that
 * does not yet have any region_page. If such a region is found,
 * a region_page is created and initialized.
 */
const int scm_create_region() {
    create_descriptor_root();

    region_t* region = NULL;
    int start = descriptor_root->next_reg_index % SCM_MAX_REGIONS;
    int i =  start;
    region = &(descriptor_root->regions[i]);
    while (region->firstPage != NULL) {
        // if the mutator calls scm_create_region() without refreshing
        // it, dc will still be 0. So if age != current_time
        // and dc == 0, we can reuse the region.
        if (region->age != descriptor_root->current_time
                && region->dc == 0) {
            region->age = descriptor_root->current_time;
            descriptor_root->next_reg_index = i;
            return (const int) i;
        }
        i = (i + 1) % SCM_MAX_REGIONS;
        if (i == start) {
#ifdef SCM_DEBUG
            printf("Region contingency exceeded.\n");
#endif
            return -1;
        }
        region = &descriptor_root->regions[i];
    }
    descriptor_root->next_reg_index = (i + 1) % SCM_MAX_REGIONS;
    region->age = descriptor_root->current_time;
    region_page_t* page = init_region_page(region);
    region->firstPage = page;
    region->next_free_address = page->memory;

// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region == NULL
            || region->firstPage == NULL
            || region->lastPage == NULL
            || region->dc != 0
            || region->number_of_region_pages != 1 ) {
        printf("Region creation or initialization failed.\n");
        exit(-1);
    }
#endif

    return (const int) i;
}

/**
 * scm_unregister_region() sets the age of the region back to a 
 * value that is not equal to the descriptor_root current_time. 
 * As a consequence the region may be reused again if its dc is 0.
 */
void scm_unregister_region(const int region) {
    if (descriptor_root == NULL) {
        return;
    }

    if (region < 0 || region >= SCM_MAX_REGIONS) {
#ifdef SCM_DEBUG
        printf("Region index is invalid.\n");
#endif
        return;
    }

    descriptor_root->regions[region].age =
        (descriptor_root->current_time - 1);
}

inline void *scm_malloc(size_t size) {
    return __wrap_malloc_internal(size);
}

/**
 * scm_malloc_in_region() allocates memory in a region.
 * It adds space for an object header to
 * the requested memory and initializes the
 * memory header.
 *
 * Every memory allocation request is aligned to
 * a word to effectively use cache lines.
 *
 * If the requested amount of memory is bigger than the
 * max region_page payload size, scm_malloc_in_region() returns NULL
 * and prints an error message.
 * If the region does not contain at least one
 * region_page it was not correctly initialized and
 * scm_malloc_in_region() returns a NULL pointer.
 */
void* scm_malloc_in_region(size_t size, const int region_index) {
    size_t requested_size = size + sizeof(object_header_t);
    unsigned int needed_space = CACHEALIGN(requested_size);

    if (needed_space > SCM_REGION_PAGE_PAYLOAD_SIZE) {
#ifdef SCM_DEBUG
        printf("The region allocator does not support memory of this size.\n");
#endif
        return NULL;
    }

    if (region_index < 0 || region_index >= SCM_MAX_REGIONS) {
#ifdef SCM_DEBUG
        printf("Region index is invalid.\n");
#endif
        return NULL;
    }

    create_descriptor_root();

    region_t* region = &descriptor_root->regions[region_index];

    if (region == NULL) {
#ifdef SCM_DEBUG
        printf("Cannot allocate into NULL region.\n");
#endif
        return NULL;
    }

    // check pre-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region->firstPage == NULL || region->lastPage == NULL) {
        printf("Region was not correctly initialized.\n");
        exit(-1);
    }
    if(region->age != descriptor_root->current_time) {
        printf("Allocation into zombie page is not allowed.\n");
        exit(-1);
    }
    region_t* invar_region = region;
#endif

    object_header_t* new_obj = region->next_free_address;
    region->next_free_address += needed_space;

    // check if the requested size fits into the region page
    if(region->next_free_address > region->last_address_in_last_page) {
        // slow allocation
#ifdef SCM_DEBUG
        printf("Page is full.\n Creating new page...[new region_page (%u)].\n", SCM_REGION_PAGE_SIZE);
#endif
        // allocate new page
        region_page_t* page = init_region_page(region);

        new_obj = (object_header_t*) page->memory;
        region->next_free_address = page->memory + needed_space;
    }

    new_obj->dc_or_region_id = region_index | HB_MASK;
    new_obj->finalizer_index = -1;

// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region != invar_region) {
        printf("The region or the first region page changed during initialization.\n");
        return NULL;
    }
    if (new_obj == NULL) {
        printf("Error during allocation. Object is NULL.\n");
        return NULL;
    }
    unsigned long not_word_aligned = (unsigned long) (region->next_free_address) % (unsigned long) sizeof(long);
    if (not_word_aligned) {
        printf("Requested memory was not word aligned.\n");
        return NULL;
    }
#endif

    return PAYLOAD_OFFSET(new_obj);
}

inline void scm_free(void *ptr) {
    __wrap_free_internal(ptr);
}

/**
 * Collects descriptors incrementally
 */
static void lazy_collect(void) {
    expire_object_descriptor_if_exists(&descriptor_root->list_of_expired_obj_descriptors);

    expire_region_descriptor_if_exists(&descriptor_root->list_of_expired_reg_descriptors);
}

/**
 * Collects descriptors all at once
 */
static void eager_collect(void) {
    while (expire_object_descriptor_if_exists(
                &descriptor_root->list_of_expired_obj_descriptors));
    while (expire_region_descriptor_if_exists(
                &descriptor_root->list_of_expired_reg_descriptors));
}

inline void scm_collect(void) {
    if (descriptor_root != NULL) {
        eager_collect();
    }
}

/**
 * Checks whether the given extension time is in the bounds of the allowed
 * extension time.
 */
static inline unsigned int check_extension(unsigned int given_extension) {
    if (given_extension > SCM_MAX_EXPIRATION_EXTENSION) {
#ifdef SCM_DEBUG
        printf("Violation of SCM_MAX_EXPIRATION_EXTENSION.\n");
#endif
        return SCM_MAX_EXPIRATION_EXTENSION;
    } else {
        return given_extension;
    }
}

/**
 * scm_refresh_with_clock() refreshes a given object with a given clock,
 * which can be different to the thread-local base clock.
 * If an object is refreshed with multiple clocks it lives
 * until all clocks ticked n times, where n is the respective extension.
 * If the object is part of a region, the region is refreshed instead.
 */
void scm_refresh_with_clock(void *ptr, unsigned int extension, const unsigned int clock) {
    MICROBENCHMARK_START

    if (ptr == NULL) {
#ifdef SCM_DEBUG
        printf("Cannot refresh NULL pointer.\n");
#endif
        return;
    }

    object_header_t *object = OBJECT_HEADER(ptr);

    // is the object allocated into a region?
    if (object->dc_or_region_id < 0) {
        int region_id = object->dc_or_region_id & ~HB_MASK;

        scm_refresh_region_with_clock(region_id, extension, clock);
    } else {
        if (object->dc_or_region_id == INT_MAX) {
#ifdef SCM_DEBUG
            printf("Descriptor counter reached max value.\n");
#endif
            return;
        }

        extension = check_extension(extension);

        if (clock < 0 || clock >= SCM_MAX_CLOCKS) {
#ifdef SCM_DEBUG
            printf("Clock is invalid.\n");
#endif
            return;
        }

        create_descriptor_root();

// check pre-conditions
#ifdef SCM_CHECK_CONDITIONS
        if (descriptor_root->current_time !=
                descriptor_root->locally_clocked_obj_buffer[clock].age ||
                descriptor_root->locally_clocked_obj_buffer[clock]
                .not_expired_length == 0) {
            printf("Cannot refresh zombie clock.\n");
            return;
        }
#endif

        atomic_int_inc((int*) & object->dc_or_region_id);
        insert_descriptor(object,
                          &descriptor_root->locally_clocked_obj_buffer[clock], extension);

#ifndef SCM_EAGER_COLLECTION
        lazy_collect();
#else
        //do nothing. expired descriptors are collected at tick
#endif
    }
    
#ifdef SCM_RECORD_MEMORY_USAGE
    print_memory_consumption();
#endif

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_refresh_with_clock")
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
 * scm_global_refresh adds extension time units + 2 to the expiration time of
 * ptr making sure that all other threads have enough time to also call
 * global_refresh(ptr, extension). If the object is part of a region, the
 * region is refreshed instead.
 */
void scm_global_refresh(void *ptr, unsigned int extension) {
    MICROBENCHMARK_START
    
    if (ptr == NULL) {
#ifdef SCM_DEBUG
        printf("Cannot refresh NULL pointer.\n");
#endif
        return;
    }

    object_header_t *object = OBJECT_HEADER(ptr);

    if (object->dc_or_region_id < 0) {
        int region_id = object->dc_or_region_id & ~HB_MASK;

        scm_global_refresh_region(region_id, extension);
    } else {
        if (object->dc_or_region_id == INT_MAX) {
#ifdef SCM_DEBUG
            printf("Descriptor counter reached max value.\n");
#endif
            return;
        }

        extension = check_extension(extension);

        create_descriptor_root();

        atomic_int_inc((int*) &object->dc_or_region_id);
        insert_descriptor(object,
                          &descriptor_root->globally_clocked_obj_buffer, extension + 2);

#ifndef SCM_EAGER_COLLECTION
        lazy_collect();
#else
        //do nothing. expired descriptors are collected at tick
#endif
    }

#ifdef SCM_RECORD_MEMORY_USAGE
    print_memory_consumption();
#endif
    
    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_global_refresh")
}

/**
 * scm_refresh_region_with_clock() refreshes a given region with a given
 * clock, which can be different from the thread-local base clock.
 * If a region is refreshed with multiple clocks it lives
 * until all clocks ticked n times, where n is the respective extension.
 */
void scm_refresh_region_with_clock(const int region_index, unsigned int extension, const unsigned int clock) {

    if (region_index < 0 || region_index >= SCM_MAX_REGIONS) {
#ifdef SCM_DEBUG
        printf("Region index is invalid.\n");
#endif
        return;
    }

    extension = check_extension(extension);

    if (clock < 0 || clock >= SCM_MAX_CLOCKS) {
#ifdef SCM_DEBUG
        printf("Clock is invalid.\n");
#endif
        return;
    }

    create_descriptor_root();

    region_t* region = &descriptor_root->regions[region_index];

    if (region->dc == INT_MAX) {
#ifdef SCM_DEBUG
        printf("Region descriptor counter reached max value.\n");
#endif
        return;
    }

#ifdef SCM_CHECK_CONDITIONS
    if (descriptor_root->current_time !=
            descriptor_root->locally_clocked_reg_buffer[clock].age ||
            descriptor_root->locally_clocked_reg_buffer[clock]
            .not_expired_length == 0) {
        printf("Cannot refresh zombie or uninitialized clock.\n");
        return;
    }
#endif

    atomic_int_inc((int*) &region->dc);
    insert_descriptor(region,
                      &descriptor_root->locally_clocked_reg_buffer[clock], extension);

#ifndef SCM_EAGER_COLLECTION
    lazy_collect();
#else
    //do nothing. expired descriptors are collected at tick
#endif

#ifdef SCM_RECORD_MEMORY_USAGE
    print_memory_consumption();
#endif
}

/**
 * scm_refresh_region() adds extension time units to
 * the expiration time of a region.
 * In a multi-clock environment, scm_refresh refreshes
 * the region with the thread-local base clock.
 */
void scm_refresh_region(const int region_index, unsigned int extension) {
    scm_refresh_region_with_clock(region_index, extension, 0);
}

/**
 * scm_global_refresh_region() adds extension time units + 2 to
 * the expiration time of a region making sure that all other threads have
 * enough time to also call scm_global_refresh_region(region_id, extension).
 */
void scm_global_refresh_region(const int region_index, unsigned int extension) {
    MICROBENCHMARK_START

    if (region_index < 0 || region_index >= SCM_MAX_REGIONS) {
#ifdef SCM_DEBUG
        printf("Region index is invalid.\n");
#endif
        return;
    }

    extension = check_extension(extension);

    create_descriptor_root();

    region_t* region = &(descriptor_root->regions[region_index]);

    if (region->dc == INT_MAX) {
#ifdef SCM_DEBUG
        printf("Region descriptor counter reached max value.\n");
#endif
        return;
    }

    atomic_int_inc((int*) &region->dc);
    insert_descriptor(region,
                      &descriptor_root->globally_clocked_reg_buffer, extension + 2);

#ifndef SCM_EAGER_COLLECTION
    lazy_collect();
#else
    //do nothing. expired descriptors are collected at tick
#endif

#ifdef SCM_RECORD_MEMORY_USAGE
    print_memory_consumption();
#endif

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_global_refresh_region")
}

/**
 * increment_and_expire() increments the current index of
 * the locally clocked descriptor buffers
 * and expires the descriptors from the last index
 */
static void increment_and_expire_clock(const unsigned int clock) {
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
void scm_tick_clock(const unsigned int clock) {
    MICROBENCHMARK_START

    if (descriptor_root == NULL) {
        return;
    }

    if (clock < 0 || clock >= SCM_MAX_CLOCKS) {
#ifdef SCM_DEBUG
        printf("Clock is invalid.\n");
#endif
        return;
    }

#ifdef SCM_DEBUG
    printf("Ticking clock: %d.\n", clock);
#endif

    increment_and_expire_clock(clock);

    if (SCM_MAX_CLOCKS > 1) {
        unsigned int rr_index = descriptor_root->round_robin;
        if (rr_index == clock) {
            rr_index = (rr_index + 1) % SCM_MAX_CLOCKS;
            if (rr_index == 0) {
                rr_index = 1;
            }
        }

#ifdef SCM_CHECK_CONDITIONS
        if (rr_index == 0 || rr_index >= SCM_MAX_CLOCKS) {
            printf("The round-robin index is %u.\n", rr_index);
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

#ifdef SCM_EAGER_COLLECTION
    eager_collect();
#else
    //we also process expired descriptors at tick
    //to get a cyclic allocation/free scheme. this is optional
    lazy_collect();
#endif

#ifdef SCM_RECORD_MEMORY_USAGE
    print_memory_consumption();
#endif

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_tick_clock")
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

    if (descriptor_root == NULL || descriptor_root->blocked) {
        return;
    }

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
            printf("The round-robin index = %u must never be 0 or >= SCM_MAX_CLOCKS.\n", rr_index);
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

#ifdef SCM_EAGER_COLLECTION
    eager_collect();
#else
    lazy_collect();
#endif

#ifdef SCM_RECORD_MEMORY_USAGE
    print_memory_consumption();
#endif

    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_global_tick")
}