#include <stdlib.h>
#include <stdio.h>
#include "stm.h"
#include "meter.h"

#define LOOPRUNS 30
#define MEMSIZE1 512
#define MEMSIZE2 256


void *pointer2 = NULL;
static int region1;

void use_some_memory() {

	//this pointer is only used within this scope
	printf("|-------------- clock1-malloc --------------|\n");

	int i=0;
	for(i=0; i<LOOPRUNS; i++) {
		void* ptr = scm_malloc_in_region(MEMSIZE1, region1);
		scm_refresh(ptr, 0);
#ifdef SCM_PRINTMEM
		inc_needed_mem(MEMSIZE1);
		print_memory_consumption();
#endif
	}
	printf("|-------------------------------------------|\n");
}

int main(int argc, char** argv) {

	int i, j;

	region1 = scm_create_region();
	const int region2 = scm_create_region();

	for (i = 0; i < 10; i++) {
		//...
		use_some_memory();
		//create new memory for pointer2
		if(i % 2 == 0) {
			printf("|-------------- clock2-malloc --------------|\n");
//			pointer2 = scm_malloc_clock(20, &clock2);
			for(j=0; j<LOOPRUNS; j++) {
				void* ptr = scm_malloc_in_region(MEMSIZE2, region2);
				scm_refresh(ptr, 0);
#ifdef SCM_PRINTMEM
				inc_needed_mem(MEMSIZE1);
				print_memory_consumption();
#endif
			}
			printf("|-------------------------------------------|\n");
		}
#ifdef SCM_PRINTMEM
		for(i=0; i<LOOPRUNS; i++) {
			inc_not_needed_mem(MEMSIZE1);
			if(i % 2 == 0) {
				inc_not_needed_mem(MEMSIZE2);
			}
			print_memory_consumption();
		}
#endif
		scm_tick();
		//...
	}

	printf("------------- deallocation: --------------\n");
	for(i=0; i<60; i++) {
#ifdef SCM_PRINTMEM
		inc_not_needed_mem(MEMSIZE1);
		if(i % 2 == 0) {
			inc_not_needed_mem(MEMSIZE2);
		}
#endif
		scm_tick();
	}
	printf("success\n");

	return 0;
}
