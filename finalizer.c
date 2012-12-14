/*
 * Copyright (c) 2010 Martin Aigner, Andreas Haas
 * http://cs.uni-salzburg.at/~maigner
 * http://cs.uni-salzburg.at/~ahaas
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

