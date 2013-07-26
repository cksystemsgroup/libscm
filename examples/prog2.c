#include <stdlib.h>
#include <stdio.h>

#include "libscm.h"

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
			for(j=0; j<LOOPRUNS; j++) {
				void* ptr = scm_malloc_in_region(MEMSIZE2, region2);
				scm_refresh(ptr, 0);
			}
			printf("|-------------------------------------------|\n");
		}
		scm_tick();
		//...
	}

	printf("------------- deallocation: --------------\n");
	for(i=0; i<60; i++) {
		scm_tick();
	}

	printf("prog2: success!\n");
	return 0;
}
