/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#include "regions.h"

/**********************************************************************/
/************************* Short-term Regions *************************/
/**********************************************************************/

/**
 * scm_create_region() returns a const integer representing a new region
 * if available and -1 otherwise. The new region is detected by scanning
 * the descriptor_root regions array for a region that
 * does not yet have any region_page. If such a region is found,
 * a region_page is created and initialized.
 */
const int scm_create_region() {
    if(descriptor_root == NULL) {
        descriptor_root = get_descriptor_root();
    }

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
            fprintf(stderr, "Region contingency exceeded.\n");
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
        fprintf(stderr, "Region creation or initialization failed\n");
        exit(errno);
    }
#endif

    return (const int) i;
}

/**
 * scm_malloc_region() allocates memory in a region.
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
 * region_pages it was not correctly initialized and
 * scm_malloc_region() returns a NULL pointer.
 */
void* scm_malloc_in_region(size_t size, const int region_index) {

#ifdef SCM_CHECK_CONDITIONS
    if (region_index < 0 || region_index >= SCM_MAX_REGIONS) {
        fprintf(stderr, "Region index is invalid.");
        exit -1;
    }
#endif

    region_t* region = &descriptor_root->regions[region_index];

    // check pre-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region == NULL) {
        fprintf(stderr, "Cannot allocate into NULL region\n");
        return NULL;
    } else {
        if (region->firstPage == NULL || region->lastPage == NULL) {
            fprintf(stderr, "Region was not correctly initialized\n");
            exit(-1);
        }
        if(region->age != descriptor_root->current_time) {
            fprintf(stderr, "Allocation into zombie page is not allowed.\n");
            exit(-1);
        }
    }
    region_t* invar_region = region;
#endif

    size_t requested_size = size + sizeof(object_header_t);
    unsigned int needed_space = CACHEALIGN(requested_size);

    object_header_t* new_obj = region->next_free_address;
    region->next_free_address = (unsigned long) new_obj + needed_space;


    // check if the requested size fits into a region page
    if(region->next_free_address > region->last_address_in_last_page) {
        // slow allocation

        if (needed_space > REGION_PAGE_PAYLOAD_SIZE) {
            fprintf(stderr, "The region allocator does not support memory of this size\n");
            return NULL;
        }
#ifdef SCM_DEBUG
        printf("Page is full\n Creating new page...");
        printf("[new_region_page (%u)]\n", SCM_REGION_PAGE_SIZE);
#endif
        // allocate new page
        region_page_t* page = init_region_page(region);

        region->next_free_address = (unsigned long) page->memory + needed_space;

        new_obj = page->memory;
        new_obj->dc_or_region_id = region_index | HB_MASK;
        new_obj->finalizer_index = -1;

// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
        if (region != invar_region) {
            fprintf(stderr, "The region or the first region-page changed during initialization\n");
            exit(-1);
        }
        if (new_obj == NULL) {
            fprintf(stderr, "Error during allocation. Object is NULL\n");
            exit(-1);
        }
        unsigned long not_word_aligned = (unsigned long)
                (region->next_free_address) % (unsigned long) sizeof(long);
        if (not_word_aligned) {
                fprintf(stderr, "Requested memory was not word aligned\n");
                exit(-1);
        }
#endif
#ifdef SCM_MEMINFO
        region->lastPage->used_memory += needed_space;
        inc_nub_regions(needed_space);
#endif

        return PAYLOAD_OFFSET(new_obj);
    }

    // fast allocation
    region->next_free_address = new_obj + (needed_space/sizeof(unsigned long));

    new_obj->dc_or_region_id = region_index | HB_MASK;
    new_obj->finalizer_index = -1;


// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region != invar_region) {
        fprintf(stderr, "The region or the first region-page changed during initialization\n");
        return NULL;
    }
    if (new_obj == NULL) {
        fprintf(stderr, "Error during allocation. Object is NULL\n");
        return NULL;
    }
    unsigned long not_word_aligned = (unsigned long)
                    (region->next_free_address) % (unsigned long) sizeof(long);
    if (not_word_aligned) {
        fprintf(stderr, "Requested memory was not word aligned\n");
        return NULL;
    }
#endif

    return PAYLOAD_OFFSET(new_obj);
}

/**
 * Creates and initializes a new region page if no other
 * region page exists or if all other region pages are full.
 * The region_page is allocated page-aligned.
 */
static region_page_t* init_region_page(region_t* region);

/**
 * init_region_page() creates and initializes a new region page if no other
 * region page exists or if all other region pages are full.
 * The region_page is allocated page-aligned.
 */
static region_page_t* init_region_page(region_t* region) {

// check pre-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region == NULL) {
        fprintf(stderr, "Cannot initialize region_page for NULL region\n");
    } else if (region->age != descriptor_root->current_time) {
        fprintf(stderr, "Initializing region page into zombie region is not allowed\n");
    }
    region_t* invar_region = region;
    region_page_t* invar_first_region_page = region->firstPage;
#endif

    region_page_t* prevLastPage = region->lastPage;

    region_page_t* new_page = descriptor_root->region_page_pool;

    if (new_page != NULL) {

        descriptor_root->region_page_pool = new_page->nextPage;
        descriptor_root->number_of_pooled_region_pages--;
#ifdef SCM_PRINTMEM
        dec_pooled_mem(sizeof (region_page_t));
#endif
    }
    else {
        new_page = __real_malloc(SCM_REGION_PAGE_SIZE);

#ifdef SCM_PRINTMEM
        inc_allocated_mem(bytes);
#endif

        if (new_page == NULL) {
            fprintf(stderr, "Memory could not be allocated.\n");
            exit(-1);
        }
    }
    memset(new_page, 0, SCM_REGION_PAGE_SIZE);

    if (prevLastPage != NULL) {
        prevLastPage->nextPage = new_page;
    }

    //region->last_address_in_last_page = (void*) (unsigned long)&new_page->memory
    region->last_address_in_last_page = (unsigned long)&new_page->memory
            + REGION_PAGE_PAYLOAD_SIZE-1;
    region->lastPage = new_page;
    region->number_of_region_pages++;

// check post-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region == NULL) {
        fprintf(stderr, "The region became NULL during initialization of a region page\n");
    } else if (region != invar_region || region->firstPage != invar_first_region_page) {
        fprintf(stderr, "The region or the first region-page changed during initialization\n");
    } else if (new_page == NULL || new_page->nextPage != NULL) {
        fprintf(stderr, "The new region page was not correctly initialized\n");
    }
#endif

    return new_page;
}

/**
 * scm_global_refresh adds extension time units + 2 to the expiration time of
 * ptr making sure that all other threads have enough time to also call
 * global_refresh(ptr, extension). If the object is part of a region, the
 * region is refreshed instead.
 */
void scm_global_refresh(void *ptr, unsigned int extension) {

    object_header_t *object = OBJECT_HEADER(ptr);

    if (object->dc_or_region_id < 0) {
#ifdef SCM_DEBUG
        printf("scm_global_refresh(%lx, %d)\n", (unsigned long) ptr, extension);
#endif

        int region_id = object->dc_or_region_id & ~HB_MASK;

        scm_global_refresh_region(region_id, extension);
    } else {

        extension = check_extension(extension);

        MICROBENCHMARK_START

#ifdef SCM_DEBUG
        printf("scm_global_refresh(%lx, %d)\n", (unsigned long) ptr, extension);
#endif

        atomic_int_inc((int*) &object->dc_or_region_id);

        insert_descriptor(object,
                          &descriptor_root->globally_clocked_obj_buffer, extension + 2);

#ifndef SCM_EAGER_COLLECTION
        scm_lazy_collect();
#else
        //do nothing. expired descriptors are collected at tick
#endif

#ifdef SCM_PRINTMEM
        print_memory_consumption();
#endif
        MICROBENCHMARK_STOP
        MICROBENCHMARK_DURATION("scm_global_refresh")
    }
}

/**
 * scm_global_refresh_region() adds extension time units + 2 to
 * the expiration time of a region making sure that all other threads have
 * enough time to also call scm_global_refresh_region(region_id, extension).
 */
void scm_global_refresh_region(const int region_id, unsigned int extension) {

#ifdef SCM_CHECK_CONDITIONS
    if (region_id < 0 || region_id >= SCM_MAX_REGIONS) {
        fprintf(stderr, "Region index is invalid.");
        exit(-1);
    }
#endif

    MICROBENCHMARK_START

#ifdef SCM_DEBUG
    printf("scm_global_refresh_region(%d, %d)\n", region_id, extension);
#endif

    extension = check_extension(extension);

    region_t* region = &(descriptor_root->regions[region_id]);
    atomic_int_inc((int*) &region->dc);

    insert_descriptor(region,
                      &descriptor_root->globally_clocked_reg_buffer, extension + 2);

#ifndef SCM_EAGER_COLLECTION
    scm_lazy_collect();
#else
    //do nothing. expired descriptors are collected at tick
#endif

#ifdef SCM_PRINTMEM
    print_memory_consumption();
#endif
    MICROBENCHMARK_STOP
    MICROBENCHMARK_DURATION("scm_global_refresh_region")
}

/**
 * scm_refresh_region() adds extension time units to
 * the expiration time of a region.
 * In a multi-clock environment, scm_refresh refreshes
 * the region with the thread-local base clock.
 */
void scm_refresh_region(const int region_id, unsigned int extension) {
    scm_refresh_region_with_clock(region_id, extension, 0);
}

/**
 * scm_refresh_region_with_clock() refreshes a given region with a given
 * clock, which can be different from the thread-local base clock.
 * If a region is refreshed with multiple clocks it lives
 * until all clocks ticked n times, where n is the respective extension.
 */
void scm_refresh_region_with_clock(const int region_id, unsigned int extension, const unsigned long clock) {

#ifdef SCM_DEBUG
    printf("scm_refresh_region_with_clock(%d, %u, %u)\n", region_id, extension, clock);
#endif

// check pre-conditions
#ifdef SCM_CHECK_CONDITIONS
    if (region_id < 0 || region_id > SCM_MAX_REGIONS) {
        fprintf(stderr, "Region id is out of range\n");
        return;
    }
#endif

    extension = check_extension(extension);

#ifdef SCM_DEBUG
    printf("region id: %d\n", region_id);
#endif

    region_t* region = &descriptor_root->regions[region_id];

#ifdef SCM_CHECK_CONDITIONS
    if (descriptor_root->current_time !=
            descriptor_root->locally_clocked_reg_buffer[clock].age ||
            descriptor_root->locally_clocked_reg_buffer[clock]
            .not_expired_length == 0) {
        fprintf(stderr, "Cannot refresh zombie or uninitialized clock");
        return;
    }
#endif
    atomic_int_inc((int*) &region->dc);
    insert_descriptor(region,
                      &descriptor_root->locally_clocked_reg_buffer[clock], extension);

#ifndef SCM_EAGER_COLLECTION
    scm_lazy_collect();
#else
    //do nothing. expired descriptors are collected at tick
#endif

#ifdef SCM_PRINTMEM
    print_memory_consumption();
#endif
}

/**
 * scm_unregister_region() sets the age of the region back to a 
 * value that is not equal to the descriptor_root current_time. 
 * As a consequence the region may be reused again if the dc is 0.
 */
void scm_unregister_region(const int region) {

#ifdef SCM_CHECK_CONDITIONS
    if (region < 0 || region >= SCM_MAX_REGIONS) {
        fprintf(stderr, "Region index is invalid.\n");
        exit(-1);
    }
#endif

    descriptor_root->regions[region].age =
        (descriptor_root->current_time - 1);
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
 * Returns if no or just one region_page
 * has been allocated in the region.
 */
static void recycle_region(region_t* region);

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

        memset(firstPage, 0, SCM_REGION_PAGE_SIZE);
        region->last_address_in_last_page =
        		(unsigned long)&firstPage->memory + REGION_PAGE_PAYLOAD_SIZE-1;

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
        unsigned long pooled_memory;

        while(p != NULL) {
        	inc_pooled_mem(SCM_REGION_PAGE_SIZE);
            dec_needed_mem(p->used_memory);
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
#ifdef SCM_PRINTMEM
            inc_freed_mem(REGION_PAFE_SIZE);
#endif
            region_page_t* next = page2free->nextPage;
            __real_free(page2free);
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
    region->last_address_in_last_page = (unsigned long)
    		&region->lastPage->memory + REGION_PAGE_PAYLOAD_SIZE;
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

    region_t* expired_region = (region_t*) get_expired_mem(list);

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

            printf("decrementing DC==%lu\n", expired_region->dc);
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