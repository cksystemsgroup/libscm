/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _FINALIZER_H_
#define	_FINALIZER_H_

#include "arch.h"
#include "object.h"

#ifndef SCM_FINALIZER_TABLE_SIZE
#define SCM_FINALIZER_TABLE_SIZE 32
#endif

int run_finalizer(object_header_t *o)
    __attribute__((visibility("hidden")));

#endif	/* _FINALIZER_H_ */