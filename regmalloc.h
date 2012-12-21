/*
 * Copyright (c) 2010 Martin Aigner, Andreas Haas, Stephanie Stroka
 * http://cs.uni-salzburg.at/~maigner
 * http://cs.uni-salzburg.at/~ahaas
 * http://cs.uni-salzburg.at/~sstroka
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
#include <string.h>
#include <unistd.h>

#ifndef REGMALLOC_H_
#define REGMALLOC_H_

#ifndef REGION_PAGE_SIZE
#define REGION_PAGE_SIZE 4096
#endif /* REGION_PAGE_SIZE */

/* The max. amount of memory that fits into a region page */
#ifndef SCM_MEMINFO
#define REGION_PAGE_PAYLOAD_SIZE (REGION_PAGE_SIZE - sizeof(void*))
#else
#define REGION_PAGE_PAYLOAD_SIZE \
			(REGION_PAGE_SIZE - sizeof(void*) - sizeof(unsigned long))
#endif

typedef struct region region_t;
typedef struct region_page region_page_t;

/**
 * region_page contains a pointer to the next region_page,
 * and a chunk of memory for allocating memory objects.
 * region_page is allocated page-aligned.
 */
struct region_page {
    region_page_t* nextPage;
    char memory[REGION_PAGE_PAYLOAD_SIZE];
};

/**
 * region contains the descriptor counter for the SCM implementation,
 * a field to count the amount of region pages and pointers to the
 * first and last region page.
 * To distinguish unused regions from used regions, the age parameter 
 * is checked against the current_time parameter of the descriptor_root.
 * Unused regions can be registered with scm_register_region().
 *
 * Fast allocation is achieved by keeping track of the next_free_address
 * and the last_address_in_last_page pointers. The next_free_address
 * pointer points to available free space the last region page.
 * The last_address_in_last_page pointer points to the last address in the
 * last region page. The next_free_address pointer can never point to an 
 * address behind the last_address_in_last_page pointer.
 */
struct region {
    unsigned int dc;
    unsigned int number_of_region_pages;
    region_page_t* firstPage;
    region_page_t* lastPage;
    unsigned int age;
    // moved free_address pointer from region page to region
    void* next_free_address;
    void* last_address_in_last_page;
};


#endif /* REGMALLOC_H_ */
