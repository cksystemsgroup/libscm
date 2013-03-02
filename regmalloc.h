/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#include <string.h>
#include <unistd.h>

#ifndef REGMALLOC_H_
#define REGMALLOC_H_

#ifndef REGION_PAGE_SIZE
#define REGION_PAGE_SIZE 4096
#endif /* REGION_PAGE_SIZE */

// The max. amount of memory that fits into a region page
#ifndef SCM_MEMINFO
    #define REGION_PAGE_PAYLOAD_SIZE (REGION_PAGE_SIZE - sizeof(void*))
#else
    #define REGION_PAGE_PAYLOAD_SIZE \
 (REGION_PAGE_SIZE - sizeof(void*) - sizeof(unsigned long))
#endif

/**
 * region_page contains a pointer to the next region_page,
 * and a chunk of memory for allocating memory objects.
 * region_page is allocated page-aligned.
 */
struct region_page_t {
    region_page_t* nextPage;
    char memory[REGION_PAGE_PAYLOAD_SIZE];
};

/**
 * region contains the descriptor counter for the SCM implementation,
 * a field to count the amount of region pages and pointers to the
 * first and last region page.
 * To distinguish unused regions from used regions, the age parameter 
 * is checked against the current_time field of the descriptor_root.
 * Unused regions can be registered with scm_register_region().
 *
 * Fast allocation is achieved by keeping track of the next_free_address
 * and the last_address_in_last_page pointers. The next_free_address
 * pointer points to available free space in the last region page.
 * The last_address_in_last_page pointer points to the last address in the
 * last region page. The next_free_address pointer can never point to an 
 * address behind the last_address_in_last_page pointer.
 */
struct region_t {
    unsigned int dc;

    unsigned int number_of_region_pages;

    region_page_t* firstPage;
    region_page_t* lastPage;

    unsigned int age;

    void* next_free_address;
    void* last_address_in_last_page;
};

#endif /* REGMALLOC_H_ */
