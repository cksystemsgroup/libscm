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
#include <stdlib.h>
#include <stdio.h>
#include "stm.h"
#include "meter.h"

#define LOOPRUNS 30
#define MEMSIZE1 512
#define MEMSIZE2 256


int main(int argc, char** argv) {

	int i, j;

	int region_array[10];

	for(i=0; i<10; i++) {
		region_array[i] = scm_create_region();
		if(region_array[i] != -1) {
			void* ptr = scm_malloc_in_region(MEMSIZE2, region_array[i]);
			//inc_needed_mem(MEMSIZE2);
			scm_refresh(ptr, 0);
		}
	}

	scm_tick();
	//inc_not_needed_mem(MEMSIZE2*10);
	//print_memory_consumption();

	if(scm_create_region() == -1) {
		printf("success\n");
	}

}
