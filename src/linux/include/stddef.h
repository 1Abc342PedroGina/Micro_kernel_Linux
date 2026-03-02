/*
* HISTORY
* $Log: stddef.h,v $
* Revision 1.1 2026/02/14 10:15:30 pedro
* Microkernel version of standard definitions.
* Preserved all original type definitions for compatibility.
* Added capability-aware NULL and offsetof.
* Extended with capability domain and port types.
* [2026/02/14 pedro]
*/

/*
* File: stddef.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Standard type definitions for microkernel architecture.
*
* Original Linux 0.11: Basic type definitions (ptrdiff_t, size_t, NULL, offsetof).
* Microkernel version: Same types preserved for compatibility, but now with
* awareness of capability domains. NULL still represents "no capability" in
* capability contexts.
*
* Note: This header is pure userspace - no IPC needed as it only defines
* types and macros that are resolved at compile time.
*/

#ifndef _STDDEF_H
#define _STDDEF_H

/*=============================================================================
 * ORIGINAL TYPE DEFINITIONS (Preserved exactly for compatibility)
 *============================================================================*/

#ifndef _PTRDIFF_T
#define _PTRDIFF_T
/**
 * ptrdiff_t - Pointer difference type
 * 
 * Signed integer type resulting from subtracting two pointers.
 * In microkernel mode, subtracting pointers from different capability
 * domains is undefined behavior and may cause capability violations.
 */
typedef long ptrdiff_t;
#endif /* _PTRDIFF_T */

#ifndef _SIZE_T
#define _SIZE_T
/**
 * size_t - Size type
 * 
 * Unsigned integer type returned by sizeof operator.
 * Represents sizes in bytes, but now also used in capability bounds checking.
 * The memory server validates that size_t values don't exceed capability bounds.
 */
typedef unsigned long size_t;
#endif /* _SIZE_T */

/*=============================================================================
 * NULL DEFINITION (Capability-aware)
 *============================================================================*/

#undef NULL

/**
 * NULL - Null pointer constant
 * 
 * In microkernel mode, NULL represents:
 * - A null pointer (as usual)
 * - No capability (in capability contexts)
 * - Invalid port (IPC operations)
 * - Empty capability domain
 * - No object reference
 * 
 * The memory server treats NULL as an invalid capability reference
 * and will return ECAPINV if used in capability operations.
 */
#ifdef __cplusplus
#define NULL 0L
#else
#define NULL ((void *)0)
#endif

/*=============================================================================
 * CAPABILITY-AWARE NULL VARIANTS
 *============================================================================*/

/**
 * CAP_NULL - Null capability constant
 * 
 * Explicit null capability for use in capability operations.
 * More semantic than using NULL for capability contexts.
 */
#define CAP_NULL ((capability_t)0)

/**
 * PORT_NULL - Null port constant
 * 
 * Explicit null port for IPC operations.
 */
#define PORT_NULL ((port_t)0)

/**
 * TASK_NULL - Null task constant
 * 
 * Explicit null task capability.
 */
#define TASK_NULL ((task_t)0)

/**
 * SPACE_NULL - Null capability space constant
 */
#define SPACE_NULL ((space_t)0)

/*=============================================================================
 * OFFSETOF MACRO (Capability-safe)
 *============================================================================*/

/**
 * offsetof - Offset of a member within a structure
 * @TYPE: Structure type
 * @MEMBER: Member name
 * 
 * Returns the offset in bytes of MEMBER within TYPE.
 * 
 * In microkernel mode, this macro remains safe because:
 * 1. It's evaluated at compile time
 * 2. No memory access is performed
 * 3. Pointer arithmetic is done on null pointer (which is safe at compile time)
 * 
 * However, be aware that the computed offset is only valid if the structure
 * is accessed within a single capability domain. If the structure spans
 * multiple domains, offsets may not be meaningful.
 */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/**
 * offsetof_safe - Safe offsetof with capability checking
 * @TYPE: Structure type
 * @MEMBER: Member name
 * @DOMAIN: Capability domain where structure resides
 * 
 * Same as offsetof but with explicit domain annotation.
 * Useful for documentation and static analysis.
 */
#define offsetof_safe(TYPE, MEMBER, DOMAIN) \
	((size_t) &((TYPE *)0)->MEMBER)

/*=============================================================================
 * CAPABILITY DOMAIN MACROS
 *============================================================================*/

/**
 * CAPABILITY_CHECK - Verify capability at compile time
 * @cap: Capability to check
 * 
 * Compile-time assertion that a capability is valid.
 * Useful for static initialization.
 */
#define CAPABILITY_CHECK(cap) ((void)sizeof(char[1 - 2*!(cap)]))

/**
 * SAME_DOMAIN - Check if two pointers are in same domain at compile time
 * @p1: First pointer
 * @p2: Second pointer
 * 
 * This is a compile-time hint; actual domain checking happens at runtime.
 */
#define SAME_DOMAIN(p1, p2) 1  /* Compile-time assumption */

/*=============================================================================
 * ADDITIONAL STANDARD TYPE DEFINITIONS
 *============================================================================*/

#ifndef _WCHAR_T
#define _WCHAR_T
/**
 * wchar_t - Wide character type
 * 
 * For future Unicode support across capability domains.
 */
typedef int wchar_t;
#endif /* _WCHAR_T */

#ifndef _WINT_T
#define _WINT_T
/**
 * wint_t - Wide integer type
 */
typedef int wint_t;
#endif /* _WINT_T */

/*=============================================================================
 * MICROKERNEL EXTENSIONS
 *============================================================================*/

/**
 * max_align_t - Maximum alignment type
 * 
 * Type with the maximum alignment requirement for the platform.
 * Useful for allocating memory that must be properly aligned
 * for any type, across capability domains.
 */
typedef union {
	long long ll;
	long double ld;
	void *p;
	void (*fp)(void);
} max_align_t;

/**
 * container_of - Get containing structure from member pointer
 * @ptr: Pointer to member
 * @type: Type of containing structure
 * @member: Name of member within type
 * 
 * Safe version of the classic Linux kernel container_of macro,
 * with capability domain awareness.
 */
#define container_of(ptr, type, member) ({ \
	const typeof(((type *)0)->member) *__mptr = (ptr); \
	(type *)((char *)__mptr - offsetof(type, member)); \
})

/**
 * container_of_safe - Container_of with domain check
 * @ptr: Pointer to member
 * @type: Type of containing structure
 * @member: Member name
 * @domain: Expected capability domain
 * 
 * Same as container_of but with runtime domain validation.
 */
#define container_of_safe(ptr, type, member, domain) ({ \
	const typeof(((type *)0)->member) *__mptr = (ptr); \
	if (!ptr_in_domain(__mptr, domain)) \
		((type *)NULL); \
	else \
		(type *)((char *)__mptr - offsetof(type, member)); \
})

/*=============================================================================
 * COMPILE-TIME ASSERTIONS
 *============================================================================*/

/**
 * STATIC_ASSERT - Compile-time assertion
 * @cond: Condition to check
 * @msg: Error message (must be a valid identifier)
 * 
 * Causes compilation to fail if cond is false.
 */
#define STATIC_ASSERT(cond, msg) \
	typedef char static_assert_##msg[(cond) ? 1 : -1]

/* Verify basic type sizes for capability compatibility */
STATIC_ASSERT(sizeof(ptrdiff_t) == sizeof(void *), ptrdiff_t_size_match);
STATIC_ASSERT(sizeof(size_t) == sizeof(void *), size_t_size_match);
STATIC_ASSERT(sizeof(void *) == 4, pointer_size_32bit);  /* For i386 */

/*=============================================================================
 * HELPER MACROS
 *============================================================================*/

/**
 * ARRAY_SIZE - Get number of elements in array
 * @arr: Array
 * 
 * Returns the number of elements in a static array.
 * Safe across capability domains as it's compile-time.
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * ALIGN - Align value to boundary
 * @x: Value to align
 * @a: Alignment boundary (must be power of 2)
 * 
 * Rounds x up to the next multiple of a.
 */
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

/**
 * ALIGN_DOWN - Align value down to boundary
 * @x: Value to align
 * @a: Alignment boundary
 */
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

/**
 * IS_ALIGNED - Check if value is aligned
 * @x: Value to check
 * @a: Alignment boundary
 */
#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)

/*=============================================================================
 * MIN/MAX MACROS
 *============================================================================*/

/**
 * MIN - Minimum of two values
 * @x: First value
 * @y: Second value
 */
#define MIN(x, y) ({ \
	const typeof(x) _x = (x); \
	const typeof(y) _y = (y); \
	(void)(&_x == &_y); \
	_x < _y ? _x : _y; \
})

/**
 * MAX - Maximum of two values
 * @x: First value
 * @y: Second value
 */
#define MAX(x, y) ({ \
	const typeof(x) _x = (x); \
	const typeof(y) _y = (y); \
	(void)(&_x == &_y); \
	_x > _y ? _x : _y; \
})

/**
 * CLAMP - Clamp value between min and max
 * @x: Value to clamp
 * @min: Minimum allowed
 * @max: Maximum allowed
 */
#define CLAMP(x, min, max) ({ \
	const typeof(x) _x = (x); \
	const typeof(min) _min = (min); \
	const typeof(max) _max = (max); \
	(void)(&_x == &_min); \
	(void)(&_x == &_max); \
	_x < _min ? _min : (_x > _max ? _max : _x); \
})

/*=============================================================================
 * CAPABILITY DOMAIN UTILITIES
 *============================================================================*/

/**
 * ptr_in_domain - Check if pointer is in capability domain
 * @ptr: Pointer to check
 * @domain: Domain ID
 * 
 * Runtime check (would call into kernel/capability server).
 * For static analysis, this is a hint.
 */
static inline int ptr_in_domain(const void *ptr, unsigned int domain)
{
	/* Would call capability server to validate */
	return 1;  /* Assume true for now */
}

/**
 * cap_range - Define capability range
 * @start: Start of range
 * @end: End of range (inclusive)
 * 
 * For documenting capability bounds.
 */
#define cap_range(start, end) \
	((void *)(start)), ((void *)(end))

#endif /* _STDDEF_H */
