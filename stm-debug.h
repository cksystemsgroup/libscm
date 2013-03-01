/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _STM_DEBUG_H
#define	_STM_DEBUG_H

#ifndef SCM_FINALZIER_TABLE_SIZE
#define SCM_FINALZIER_TABLE_SIZE 32
#endif /*SCM_FINALZIER_TABLE_SIZE*/

#ifdef SCM_PRINTMEM
#define SCM_CALCMEM
#endif

#ifdef SCM_PRINTOVERHEAD
#define SCM_CALCOVERHEAD 1
#endif


/* scm_register_finalizer is used to register a finalizer function in
 * libscm. A function id is returned for later use. (see scm_set_finalizer)
 *
 * It is up to the user to design the scm_finalizer function. If
 * scm_finalizer returns non-zero, the object will not be deallocated.
 * libscm provides the pointer to the object as parameter of scm_finalizer.
 */
int scm_register_finalizer(int(*scm_finalizer)(void*));

/*
 * scm_set_finalizer can be used to bind a finalizer function id
 * (returned by scm_register_finalizer) to an object (ptr).
 * This function will be executed just before an expired object is
 * deallocated.
 */
void scm_set_finalizer(void *ptr, int scm_finalizer_id);

/*
 * struct scm_mem_info is used to fetch information about memory 
 * consumption during runtime.
 */
struct scm_mem_info {
	unsigned long allocated;/* total allocated bytes */
	unsigned long freed;	/* total freed bytes */
	unsigned long overhead;	/* overhead by LIBSCM */
        unsigned long num_alloc;/* total of allocated obj. */
        unsigned long num_freed;/* total of freed objects */
};

/*
 * scm_get_mem_info is used to query the contents of a
 * struct scm_mem_info from LIBSCM
 */
void scm_get_mem_info(struct scm_mem_info *info);

#endif	/* _STM_DEBUG_H */
