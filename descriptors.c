/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#include "descriptors.h"

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
            perror("Allocation of new descriptor page failed.");
            return NULL;
        }

#ifdef SCM_PRINTOVERHEAD
        inc_overhead(__real_malloc_usable_size(new_page));
#endif
    }
#ifdef SCM_PRINTMEM
    inc_allocated_mem(__real_malloc_usable_size(new_page));
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
            printf("decrementing DC==%d\n", expired_object->dc_or_region_id);
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

    if (page->number_of_descriptors == DESCRIPTORS_PER_PAGE) {
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

/**
 * Recycles a region in O(1) by pooling
 * the list of free region_pages except the
 * first region page iff the region_page_pool
 * limit is not exceeded, otherwise the region_pages
 * except the first one are deallocated and
 * the memory is handed back to the OS in O(n),
 * n = amount of region pages - 1.
 *
 * The remaining first region page indicates that the region
 * once existed, which is necessary to differentiate
 * it from regions which have not yet been used.
 * This indicates how many not-yet-used regions
 * are available.
 *
 * If the region was unregistered, all region pages
 * are recycled or deallocated.
 *
 * Returns if no or just one region_page
 * has been allocated in the region.
 */
static void recycle_region(region_t* region) {

#ifdef SCM_DEBUG
    printf("Recycle region: %p \n", region);
#endif

// check pre-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region == NULL) {
        fprintf(stderr, "Region recycling failed: NULL region should not appear in the descriptor buffers.");
        exit(-1);
    } else if (region->firstPage == NULL || region->lastPage == NULL) {
        fprintf(stderr, "Region recycling failed: Descriptor points to a region which was not correctly initialized.");
        exit(-1);
    }
    if (region->dc != 0) {
        fprintf(stderr, "Region recycling failed: Region seems to be still alive.");
        exit(-1);
    }
    region_t* invar_region = region;
#endif

    region_page_t* legacy_pages;
    unsigned long number_of_recycle_region_pages;

    // if the region has been used in the current thread...
    if (region->age == descriptor_root->current_time) {
        //.. recycle everything except the first page
        region_page_t* firstPage = region->firstPage;
        legacy_pages = firstPage->nextPage;

        memset(firstPage, '\0', SCM_REGION_PAGE_SIZE);
        region->last_address_in_last_page =
                &firstPage->memory + SCM_REGION_PAGE_PAYLOAD_SIZE;

        // nothing to put into the pool
        if (legacy_pages == NULL) {

// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
            if (region->number_of_region_pages != 1) {
                fprintf(stderr, "Region recycling failed: Number of region pages is %u, but only one "
                        "region page exists\n",
                        region->number_of_region_pages);
                exit(-1);
            } else {
                if (region->firstPage != region->lastPage) {
                    fprintf(stderr, "Region recycling failed: Last region page is not equal to first "
                            "region page, but only one region page exists\n");
                    exit(-1);
                }
                if (region != invar_region) {
                    fprintf(stderr, "Region recycling failed: The region changed during recycling\n");
                    exit(-1);
                }
                if(region->firstPage->nextPage != NULL) {
                    fprintf(stderr, "Region recycling failed: Next page pointer is corrupt: %p\n",
                            region->firstPage->nextPage);
                    exit(-1);
                }
            }
#endif
            region->dc = 0;

            return;
        }


// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
        if (region->number_of_region_pages <= 1) {
            fprintf(stderr, "Region recycling failed: Number of region pages is %u, "
                    "but more than 1 region pages were expected.\n",
                    region->number_of_region_pages);
            exit(-1);
        }
#endif

        number_of_recycle_region_pages =
            region->number_of_region_pages - 1;

    }
    // if the region was a zombie in the current thread...
    else {
#ifdef SCM_DEBUG
        printf("Region expired\n");
#endif
        //.. recycle everything, also the first page
        legacy_pages = region->firstPage;

        // nothing to put into the pool
        if (legacy_pages == NULL) {


// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
            if (region->number_of_region_pages != 0) {
                fprintf(stderr, "Region recycling failed: "
                        "Number of region pages is not zero, but no region pages exist\n");
                exit(-1);
            } else {
                if (region->firstPage != region->lastPage) {
                    fprintf(stderr, "Region recycling failed: "
                            "Last region page is not equal to first region page, "
                            "but only one region page exists\n");
                    exit(-1);
                }
                if (region != invar_region) {
                    fprintf(stderr, "Region recycling failed: The region changed during recycling\n");
                    exit(-1);
                }
            }
#endif
            region->dc = 0;

            return;
        }

// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
        if (region->number_of_region_pages == 0) {
            fprintf(stderr, "Region recycling failed: "
                    "Number of region pages is %u, but legacy pages "
                    "could be obtained\n",
                    region->number_of_region_pages);
            exit(-1);
        }
#endif

        // a zombie region recycles all region pages
        number_of_recycle_region_pages =
            region->number_of_region_pages;
    }

    unsigned long number_of_pooled_region_pages =
        descriptor_root->number_of_pooled_region_pages;

    // is there space in the region page pool?
    if ((number_of_pooled_region_pages
            + number_of_recycle_region_pages) <=
            SCM_REGION_PAGE_FREELIST_SIZE) {
        //..yes, there is space in the region page pool

#ifdef SCM_PRINTMEM
        region_page_t* p = legacy_pages;

        while(p != NULL) {
            inc_pooled_mem(SCM_REGION_PAGE_SIZE);
            p = p->nextPage;
        }
#endif
#ifdef SCM_PRINTOVERHEAD
        region_page_t* p2 = legacy_pages;
        
        while(p2 != NULL) {
            inc_overhead(__real_malloc_usable_size(p2));
            p2 = p2->nextPage;
        }
#endif

        region_page_t* first_in_pool = descriptor_root->region_page_pool;
        region_page_t* last_page = region->lastPage;

        last_page->nextPage = first_in_pool;
        descriptor_root->region_page_pool = legacy_pages;
        descriptor_root->number_of_pooled_region_pages =
            number_of_pooled_region_pages + number_of_recycle_region_pages;

    } else {

        //..no, there is no space in the region page pool
        // If the first region page is not NULL,
        // deallocate all region pages
        region_page_t* page2free = legacy_pages;

        while (page2free != NULL && (number_of_pooled_region_pages
                + number_of_recycle_region_pages) >
                SCM_REGION_PAGE_FREELIST_SIZE) {
            region_page_t* next = page2free->nextPage;
            __real_free(page2free);

#ifdef SCM_PRINTMEM
            inc_freed_mem(SCM_REGION_PAGE_SIZE);
#endif

            page2free = next;

            number_of_recycle_region_pages--;
        }


        // check again if we can recycle now
        if (page2free != NULL && (number_of_pooled_region_pages
                + number_of_recycle_region_pages) <=
                SCM_REGION_PAGE_FREELIST_SIZE) {
            
            region_page_t* first_in_pool = descriptor_root->region_page_pool;
            region_page_t* last_page = region->lastPage;
            if(last_page != NULL) {
                last_page->nextPage = first_in_pool;
                descriptor_root->region_page_pool = page2free;
                descriptor_root->number_of_pooled_region_pages =
                        number_of_recycle_region_pages - 1;

#ifdef SCM_PRINTMEM
                inc_pooled_mem(number_of_recycle_region_pages * SCM_REGION_PAGE_SIZE);
#endif
            }
        }

        descriptor_root->number_of_pooled_region_pages =
                number_of_pooled_region_pages + number_of_recycle_region_pages;
    }

    region->number_of_region_pages = 1;
    region->lastPage = region->firstPage;
    region->last_address_in_last_page =
            &region->lastPage->memory + SCM_REGION_PAGE_PAYLOAD_SIZE;
    region->next_free_address = &region->lastPage->memory;

// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
        if (region->number_of_region_pages != 1) {
            fprintf(stderr, "Region recycling failed: "
                    "Number of region pages is %u, but only one region page exists.\n", 
                    region->number_of_region_pages);
            exit(-1);
        } else {
            if (region->firstPage != region->lastPage) {
                fprintf(stderr, "Region recycling failed: "
                    "Last region page is not equal to first region page, "
                    "but only one region page should exist.\n");
                exit(-1);
            }
            if (region != invar_region) {
                fprintf(stderr, "Region recycling failed: "
                    "The region changed during recycling.\n");
                exit(-1);
            }
        }
#endif
    if (region->age != descriptor_root->current_time) {

        region->number_of_region_pages = 0;
        region->lastPage = region->firstPage = NULL;

// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
        if (region->number_of_region_pages != 0) {
            fprintf(stderr, "Region recycling failed: "
                    "Number of region pages is %u, but no region pages should exist.\n", 
                    region->number_of_region_pages);
            exit(-1);
        } else {
            if (region->firstPage != NULL) {
                fprintf(stderr, "Region recycling failed: "
                        "First page is not null as expected\n");
                exit(-1);
            }
            if (region != invar_region) {
                fprintf(stderr, "Region recycling failed: "
                        "The region changed during recycling\n");
                exit(-1);
            }
        }
#endif
    }
}

/*
 * Expires a region descriptor and decrements its descriptor counter. When the
 * descriptor counter is 0, the region to which the descriptor points to is recycled.
 * Returns 0 iff no more expired region descriptors exist.
 */
int expire_region_descriptor_if_exists(expired_descriptor_page_list_t *list) {

// check pre-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (list == NULL) {
        perror("Expired descriptor page list is NULL, but was expected to exist");
        return 0;
    }
#endif

    region_t* expired_region = (region_t*) get_expired_memory(list);

    if (expired_region != NULL) {
        if (atomic_int_dec_and_test((volatile int*) & expired_region->dc)) {

#ifdef SCM_DEBUG
            printf("region FREE(%lx)\n",
                   (unsigned long) expired_region);
#endif

            recycle_region(expired_region);

// optimization: avoiding else conditions
#ifdef SCM_DEBUG
        } else {

            printf("decrementing DC==%u\n", expired_region->dc);
#endif
        }
        return 1;
    } else {
#ifdef SCM_DEBUG
        printf("no expired object found\n");
#endif
        return 0;
    }
}