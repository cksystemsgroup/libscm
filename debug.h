/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _STM_DEBUG_H
#define	_STM_DEBUG_H

#ifndef SCM_FINALIZER_TABLE_SIZE
#define SCM_FINALIZER_TABLE_SIZE 32
#endif /*SCM_FINALIZER_TABLE_SIZE*/

/** scm_register_finalizer registers a finalizer function in
 * libscm. A function id is returned for later use. (see scm_set_finalizer)
 *
 * It is up to the user to design the scm_finalizer function. If
 * scm_finalizer returns non-zero, the object will not be deallocated.
 * libscm provides the pointer to the object as parameter of scm_finalizer.
 */
int scm_register_finalizer(int(*scm_finalizer)(void*));

/**
 * scm_set_finalizer binds a finalizer function id
 * (returned by scm_register_finalizer) to an object (ptr).
 * This function will be executed just before an expired object is
 * deallocated.
 */
void scm_set_finalizer(void *ptr, int scm_finalizer_id);

#endif	/* _STM_DEBUG_H */
