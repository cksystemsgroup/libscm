/*
 * Copyright (c) 2010, the Short-term Memory Project Authors.
 * All rights reserved. Please see the AUTHORS file for details.
 * Use of this source code is governed by a BSD license that
 * can be found in the LICENSE file.
 */

#ifndef _DEBUG_H_
#define	_DEBUG_H_

#ifdef SCM_DEBUG_THREADS
#include <pthread.h>

#define printf printf("%p: ", pthread_self());printf
#endif

#endif /* _DEBUG_H_ */