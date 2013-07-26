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

	//setbuf(stdout, NULL); //do not buffer stdout

	region1 = scm_create_region();
	const int region2 = scm_create_region();

	const int clock1 = scm_register_clock();
	const int clock2 = scm_register_clock();

	if((clock2-1) != clock1) {
		printf("1) Error while creating clocks: %d != %d\n", clock1, clock2);
		exit(0);
	}

	if(clock1 < 0 || clock2 < 0) {
		printf("2) Error while creating clocks\n");
		exit(0);
	}

	for (i = 0; i < 10; i++) {
		//...
		use_some_memory();
		//create new memory for pointer2
		if(i % 2 == 0) {
			printf("|-------------- clock2-malloc --------------|\n");
			for(j=0; j<LOOPRUNS; j++) {
				void* ptr = scm_malloc_in_region(MEMSIZE2, region2);
				scm_refresh(ptr, 0);
				scm_refresh_with_clock(ptr, clock1, 0);
			}
			printf("|-------------------------------------------|\n");
		}
		scm_tick();
		//...
	}

	printf("------------- second clock tick: --------------\n");
	for(i = 0; i < 10; i++) {
		if(i % 2 == 0) {
			for(i=0; i<LOOPRUNS; i++) {
				scm_tick_clock(clock1);
			}
		}
	}

	for(i = 0; i < 61; i++) {
		scm_tick();
	}

	printf("prog3: success!\n");
	return 0;
}