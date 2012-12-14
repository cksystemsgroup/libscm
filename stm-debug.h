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
