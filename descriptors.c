/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "stm.h"
#include "scm-desc.h"
#include "regmalloc.h"
#include "meter.h"
#include "arch.h"

/**
 * Returns a descriptor page from the descriptor page
 * pool or allocates a new descriptor page if the
 * descriptor page pool is empty.
 */
static descriptor_page_t *new_descriptor_page() {

    descriptor_page_t *new_page = NULL;

    if (descriptor_root->number_of_pooled_descriptor_pages > 0) {
        descriptor_root->number_of_pooled_descriptor_pages--;
        new_page = descriptor_root->descriptor_page_pool
                   [descriptor_root->number_of_pooled_descriptor_pages];

#ifdef SCM_PRINTMEM
        dec_pooled_mem(sizeof(descriptor_page_t));
#endif
    } else {
        new_page = __real_malloc(SCM_DESCRIPTOR_PAGE_SIZE);

        if (!new_page) {
            perror("Allocation of new_descriptor_page failed.");
            return NULL;
        }

#ifdef SCM_PRINTOVERHEAD
        inc_overhead(__real_malloc_usable_size(new_page));
#endif
    }
#ifdef SCM_PRINTMEM
    inc_alloc_mem(sizeof(descriptor_page_t));
#endif

    new_page->number_of_descriptors = 0;
    new_page->next = NULL;

    return new_page;
}

static inline void recycle_descriptor_page(descriptor_page_t *page) {

    if (descriptor_root->number_of_pooled_descriptor_pages <
            SCM_DESCRIPTOR_PAGE_FREELIST_SIZE) {

        descriptor_root->descriptor_page_pool[descriptor_root->number_of_pooled_descriptor_pages] = page;
    
        descriptor_root->number_of_pooled_descriptor_pages++;

#ifdef SCM_PRINTMEM
        inc_pooled_mem(sizeof(descriptor_page_t));
#endif
    } else {
#ifdef SCM_PRINTOVERHEAD
        dec_overhead(__real_malloc_usable_size(page));
#endif

#ifdef SCM_PRINTMEM
        inc_freed_mem(__real_malloc_usable_size(page));
#endif

        __real_free(page);
    }
}

/**
 * get_expired_memory() returns an expired object or region
 * from the expired descriptor page.
 */
static void* get_expired_memory(expired_descriptor_page_list_t *list) {

    //remove from the first page
    descriptor_page_t *page = list->first;

    if (page == NULL) {
        //list is empty
        return NULL;
    }

#ifdef SCM_DEBUG
    printf("list->collected: %lu, page->num_of_desc: %lu\n", list->collected,
     page->number_of_descriptors);
#endif

    if (list->collected == page->number_of_descriptors) {
        //page has already been emptied
        //free this page and proceed with next one at index 0
        list->collected = 0;

        if (list->first == list->last) {
            //this was the last page in list
            recycle_descriptor_page(list->first);

            list->first = NULL;
            list->last = NULL;

            return NULL;
        } else {
            //there are more pages left, remove empty list->first from list
            page = list->first->next;

            recycle_descriptor_page(list->first);

            list->first = page;
        }
    }
#ifdef SCM_DEBUG
    if (list->collected == page->number_of_descriptors) {
        printf("more than one empty page in list\n");
        return NULL;
    }
#endif

    void* expired_memory = page->descriptors[list->collected];

    list->collected++;

    return expired_memory;
}

/*
 * Expires an object descriptor and decrements the object's descriptor counter.
 * If the descriptor counter is 0, the object to which the descriptor points
 * is deallocated. Returns 0 iff no more expired object descriptors exist.
 */
int expire_object_descriptor_if_exists(expired_descriptor_page_list_t *list) {

// check pre-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (list == NULL) {
        perror("Expired descriptor page list is NULL, but was expected to exist");
        return 0;
    }
#endif

    object_header_t *expired_object = (object_header_t*) get_expired_memory(list);

    if (expired_object != NULL) {
        //decrement the descriptor counter of the expired object
        if (atomic_int_dec_and_test((int*) &expired_object->dc_or_region_id)) {
            //with the descriptor counter now zero run finalizer and free it
            int finalizer_result = run_finalizer(expired_object);

            if (finalizer_result != 0) {
#ifdef SCM_DEBUG
                printf("WARNING: finalizer returned %d\n", finalizer_result);
                printf("WARNING: %lx is a leak\n",
                       (unsigned long) PAYLOAD_OFFSET(expired_object));
#endif

                return 1; //do not free the object
            }

#ifdef SCM_DEBUG
            printf("object FREE(%lx)\n",
                   (unsigned long) PAYLOAD_OFFSET(expired_object));
#endif

#ifdef SCM_PRINTOVERHEAD
            dec_overhead(sizeof (object_header_t));
#endif

#ifdef SCM_PRINTMEM
            inc_freed_mem(__real_malloc_usable_size(expired_object));
#endif
            __real_free(expired_object);

            return 1;
        } else {
#ifdef SCM_DEBUG
            printf("decrementing DC==%u\n", expired_object->dc_or_region_id);
#endif
            return 1;
        }
    } else {
#ifdef SCM_DEBUG
        printf("no expired object found\n");
#endif
        return 0;
    }
}

/*
 * Inserts a descriptor for the object or region
 * provided as parameter 'ptr' */
void insert_descriptor(void* ptr, descriptor_buffer_t *buffer,
                       unsigned int expiration) {

    unsigned int insert_index = (buffer->current_index + expiration) % buffer->not_expired_length;

    descriptor_page_list_t *list = &buffer->not_expired[insert_index];

    if (list->first == NULL) {
        list->first = new_descriptor_page();
        list->last = list->first;
    }

    //insert in the last page
    descriptor_page_t *page = list->last;

    if (page->number_of_descriptors == SCM_DESCRIPTORS_PER_PAGE) {
        //page is full. create new page and append to end of list
    	page = new_descriptor_page();
        list->last->next = page;
        list->last = page;
    }

    page->descriptors[page->number_of_descriptors] = ptr;
    page->number_of_descriptors++;
}

/*
 * Appends a descriptor buffer to the expired page list.
 * expire_buffer always operates on the current_index-1 list of the buffer
 */
void expire_buffer(descriptor_buffer_t *buffer,
                   expired_descriptor_page_list_t *exp_list) {

    int to_be_expired_index = buffer->current_index - 1;

    if (to_be_expired_index < 0)
        to_be_expired_index += buffer->not_expired_length;

    descriptor_page_list_t *just_expired_page_list = &buffer->not_expired[to_be_expired_index];

    if (just_expired_page_list->first != NULL) {

        if (just_expired_page_list->first->number_of_descriptors != 0) {

            //append page_list to expired_page_list
            if (exp_list->first == NULL) {
                exp_list->first = just_expired_page_list->first;
                exp_list->last = just_expired_page_list->last;
                exp_list->collected = 0;
            } else {
                exp_list->last->next = just_expired_page_list->first;
                exp_list->last = just_expired_page_list->last;
            }

            //reset just_expired_page_list
            just_expired_page_list->first = NULL;
            just_expired_page_list->last = just_expired_page_list->first;
        } else {
            //leave empty just_expired_page_list where it is
        }
    } else {
        //buffer to expire is empty
    }
}

/**
 * Increments the current_index modulo the maximal expiration extension.
 */
inline void increment_current_index(descriptor_buffer_t *buffer) {
    buffer->current_index = (buffer->current_index + 1) % buffer->not_expired_length;
}
