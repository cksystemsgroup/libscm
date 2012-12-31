/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#include "stm-debug.h"
#include "arch.h"
#include "scm-desc.h"


//finalizer table contains 100 function pointers;
static int (*finalizer_table[SCM_FINALZIER_TABLE_SIZE])(void*);

//bump pointer on the finalizer table
static int finalizer_index = 0;

int scm_register_finalizer(int(*scm_finalizer)(void*)) {

    int index = atomic_int_exchange_and_add(&finalizer_index, 1);

    if (index >= SCM_FINALZIER_TABLE_SIZE) return -1; //error, table full

    finalizer_table[index] = scm_finalizer;
    return index;
}

void scm_set_finalizer(void *ptr, int scm_finalizer_id) {
    //set function index
    object_header_t *o = OBJECT_HEADER(ptr);
    o->finalizer_index = scm_finalizer_id;
}


int run_finalizer(object_header_t *o) {

    //INVARIANT: object o is already expired

    if (o->finalizer_index == -1) return 0; //object has no finalizer

    void *ptr = PAYLOAD_OFFSET(o);
    int (*finalizer)(void*);
    //get function pointer to objects finalizer
    finalizer = finalizer_table[o->finalizer_index];

    //run finalizer and return the result of it
    return (*finalizer)(ptr);
}

