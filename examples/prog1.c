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

	scm_register_thread();
	
	int i;
	
	for (i = 0; i < 10; i++) {
		//...
		use_some_memory();
		scm_tick();
		//...
	}
	
	return 0;
}