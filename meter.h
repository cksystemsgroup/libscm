/*
 * Copyright (c) 2010 Martin Aigner, Andreas Haas, Stephanie Stroka
 * http://cs.uni-salzburg.at/~maigner
 * http://cs.uni-salzburg.at/~ahaas
 * http://cs.uni-salzburg.at/~sstroka
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

#ifndef _METER_H
#define	_METER_H

#ifdef SCM_PRINTOVERHEAD
void inc_overhead(long inc) __attribute__ ((visibility("hidden")));
void dec_overhead(long inc) __attribute__ ((visibility("hidden")));
#endif

#ifdef SCM_PRINTMEM

/**
 * Keep track of the pooled/unpooled memory
 */
void dec_pooled_mem(int inc);
void inc_pooled_mem(int inc);

/**
 * Keeps track of the freed memory
 */
void inc_freed_mem(int inc);
/**
 * Keeps track of the allocated memory
 */
void inc_allocated_mem(int inc);

/**
 * Keeps track of the not needed memory
 */
void inc_not_needed_mem(int inc);
/**
 * Keeps track of the needed memory
 */
void inc_needed_mem(int inc);

void enable_mem_meter(void) __attribute__ ((visibility("hidden")));
void disable_mem_meter(void) __attribute__ ((visibility("hidden")));

/**
 * prints the memory consumption for needed and allocated memory
 */
void print_memory_consumption(void);
#endif

#endif	/* _METER_H */

