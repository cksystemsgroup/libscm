#include <stdlib.h>

#include "libscm.h"

void *pointer2 = NULL;

void use_some_memory() {
	//this pointer is only used within this scope
	void *pointer1 = scm_malloc(10);

	//let pointer1 expire after this round
	scm_refresh(pointer1, 0);
	
	if (pointer2 != NULL) {
		//memory at pointer2 from previous round
		//.. do something with pointer2
	}
	
	//create new memory for pointer2
	pointer2 = scm_malloc(20);
	
	//refresh pointer2 to be vaid in the next round
	scm_refresh(pointer2, 1);
}

int main(int argc, char** argv) {

	const int region_index = scm_create_region();
	void* ptr = scm_malloc_in_region(10, region_index);

	int i;
	
	for (i = 0; i < 10; i++) {
		//...
		use_some_memory();
		scm_tick();
		//...
	}
	
	printf("prog1: success!\n");
	return 0;
}