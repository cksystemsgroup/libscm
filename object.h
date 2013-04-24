/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _OBJECT_H_
#define	_OBJECT_H_

/*
 * objects allocated through libscm have an additional object header that
 * is located before the chunk storing the object.
 *
 * -------------------------  <- pointer to object_header_t
 * | descriptor counter OR |
 * | region id AND         |
 * | finalizer index       |
 * -------------------------  <- pointer to the payload data that is
 * | payload data          |     returned to the user
 * ~ returned to user      ~
 * |                       |
 * -------------------------
 *
 */
typedef struct object_header object_header_t;

struct object_header {
    // Depending on whether the region-based allocator is
    // used or not, the following field will be positive or negative.
    // A negative value indicates region allocation. Resetting the
    // hsb returns the region id.
    int dc_or_region_id;
    // finalizer_index must be signed so that a finalizer_index
    // may be set to -1 indicating that no finalizer exists
    int finalizer_index;
};

#define OBJECT_HEADER(_ptr) \
    (object_header_t*)(_ptr - sizeof(object_header_t))
#define PAYLOAD_OFFSET(_o) \
    ((void*)(_o) + sizeof(object_header_t))

#endif	/* _OBJECT_H_ */