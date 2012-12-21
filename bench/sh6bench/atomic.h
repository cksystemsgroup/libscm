#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#include <stdint.h>

#ifdef __i386__

static __inline__ uint64_t rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}

#define COUNTERSIZE 32

typedef uint64_t atomic_value;

static inline uint64_t get_atomic_value(void *pointer, uint32_t counter) {
	uint64_t atomic_value = 0;
	atomic_value = (uint32_t)pointer;
	atomic_value <<= 32;
	atomic_value |= counter;
	return atomic_value;
}

static inline void *atomic_value_to_pointer(uint64_t atomic_value) {
	return (void *)(uint32_t)(atomic_value>>32);
} 

static inline uint32_t atomic_value_to_counter(uint64_t atomic_value) {
	return (uint32_t)atomic_value & 0xffff;
}

#define LOCK_PREFIX "lock; "

static inline int atomic_add_return(int i, int *val)
{
        int __i;
        /* Modern 486+ processor */
        __i = i;
        __asm__ __volatile__(
                "lock xaddl %0, %1"
                :"+r" (i), "+m" (*val)
                : : "memory");
        return i + __i;
}

static __inline__ void atomic_inc(int *v)
{
        __asm__ __volatile__(
                LOCK_PREFIX "incl %0"
                :"+m" (*v));
}

#define atomic_inc_return(val) atomic_add_return(1, val)

/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1.
 */ 
static __inline__ void atomic_dec(int *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "decl %0"
		:"+m" (*v));
}


struct __xchg_dummy { unsigned long a[100]; };
#define __xg(x) ((struct __xchg_dummy *)(x))

static inline long long __cmpxchg64(uint64_t *ptr, uint64_t old, uint64_t new)
{
	unsigned long long prev;
	__asm__ __volatile__(LOCK_PREFIX "cmpxchg8b %3"
			     : "=A"(prev)
			     : "b"((unsigned long)new),
			       "c"((unsigned long)(new >> 32)),
			       "m"(*__xg(ptr)),
			       "0"(old)
			     : "memory");
	return prev;
}


#define cmpxchg64(ptr,o,n)\
	((__typeof__(*(ptr)))__cmpxchg64((ptr),(unsigned long long)(o),\
					(unsigned long long)(n)))

#define ARCH_HAS_CAS64 1
#define USE_RDTSC 1

static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, int size)
{
	unsigned long prev;
	switch (size) {
	case 1:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgb %b1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 2:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgw %w1,%2"
				     : "=a"(prev)
				     : "r"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 4:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgl %1,%2"
				     : "=a"(prev)
				     : "r"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	}
	return old;
}
#define cmpxchg(ptr,o,n)\
	((__typeof__(*(ptr)))__cmpxchg((ptr),(unsigned long)(o),\
					(unsigned long)(n),sizeof(*(ptr))))



#elif defined(__x86_64__)

static __inline__ uint64_t rdtsc(void)
{
  unsigned int hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );
}

#define COUNTERSIZE 64

typedef __uint128_t atomic_value;

static inline atomic_value get_atomic_value(void *pointer, uint64_t counter) {
	atomic_value avalue = 0;
	avalue = (uint64_t)pointer;
	avalue <<= 64;
	avalue |= counter;
	return avalue;
}

static inline void *atomic_value_to_pointer(atomic_value avalue) {
	return (void *)(uint64_t)(avalue>>64);
}

static inline uint64_t atomic_value_to_counter(atomic_value avalue) {
	return (uint64_t)avalue & 0xffffffff;
}

// CAS on double words with embedded counters. Increments the embedded counter automatically.
// The fourth argument is the increment to the counter, which defaults to 1 using the macro below.
static inline int atomic_compare_and_swap_inc(volatile atomic_value* ptr, atomic_value old, atomic_value new, int inc, ...) {
  atomic_value new_value = get_atomic_value(atomic_value_to_pointer(new), atomic_value_to_counter(old) + inc);
  return __sync_bool_compare_and_swap(ptr, old, new_value);
}
#define atomic_compare_and_swap_inc(ptr, old, new, ...) atomic_compare_and_swap_inc(ptr, old, new, ##__VA_ARGS__, 1)

#define LOCK_PREFIX "lock; "

static __inline__ void atomic_inc(int *v)
{
        __asm__ __volatile__(
                LOCK_PREFIX "incl %0"
                :"+m" (*v));
}

static __inline__ void atomic_dec(int *v)
{
        __asm__ __volatile__(
                LOCK_PREFIX "decl %0"
                :"+m" (*v));
}

static inline int atomic_add_return(int i, int *val)
{
        int __i;
        /* Modern 486+ processor */
        __i = i;
        __asm__ __volatile__(
                "lock xaddl %0, %1"
                :"+r" (i), "+m" (*val)
                : : "memory");
        return i + __i;
}


#define atomic_inc_return(v)  (atomic_add_return(1,v))
#define atomic_dec_return(v)  (atomic_sub_return(1,v))

#else

#warning Unsupported CPU architecture, using unoptimized bitmap handling

#endif
#endif
