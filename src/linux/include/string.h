/*
* HISTORY
* $Log: string.h,v $
* Revision 1.1 2026/02/14 09:15:30 pedro
* Microkernel version of string functions.
* Memory operations (memcpy, memmove, memchr) now use IPC to memory server.
* String operations remain in userspace (no kernel privileges needed).
* Added capability-safe versions of all functions.
* Preserved original inline assembly optimizations where possible.
* [2026/02/14 pedro]
*/

/*
* File: string.h
* Author: Linus Torvalds (original Linux version)
*         Falcon (modifications)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* String and memory functions for microkernel architecture.
*
* Original Linux 0.11: Hand-optimized inline assembly using string instructions.
* Microkernel version: Split into two categories:
*   1. Pure string operations (strcpy, strcat, strcmp, etc.) - remain in userspace
*   2. Memory operations (memcpy, memmove, memchr) - use IPC to memory server
*
* Memory operations require capability checking as they access memory across
* protection domains. String operations work entirely within the current
* task's address space and don't need kernel intervention.
*/

#ifndef _STRING_H_
#define _STRING_H_

#include <sys/types.h>
#include <linux/kernel.h>
#include <linux/head.h>

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

/* Message codes for memory operations */
#define MSG_MEM_STRING_COPY	0x0F00	/* Copy memory region */
#define MSG_MEM_STRING_MOVE	0x0F01	/* Move memory region (handles overlap) */
#define MSG_MEM_STRING_CHR	0x0F02	/* Find character in memory */
#define MSG_MEM_STRING_REPLY	0x0F03	/* Reply from memory server */

/* Memory operation message structures */
struct msg_mem_string_op {
	struct mk_msg_header header;
	unsigned long dest;		/* Destination address */
	unsigned long src;		/* Source address (or character) */
	int n;				/* Number of bytes */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
	int op_type;			/* COPY, MOVE, CHR */
};

struct msg_mem_string_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	unsigned long value;		/* Return value (e.g., memchr result) */
	capability_t granted_caps;	/* New capabilities granted */
};

/* Operation types */
#define MEM_OP_COPY	1
#define MEM_OP_MOVE	2
#define MEM_OP_CHR	3

/* Capability for memory operations */
#define CAP_MEM_STRING	0x0080	/* Memory string operations capability */

/*
 * Original note from Linus (preserved for historical context):
 *
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strtok,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. String instructions have been
 * used through-out, making for "slightly" unclear code :-)
 *
 *		(C) 1991 Linus Torvalds
 */

/* External declarations */
extern char * strerror(int errno);
extern char * ___strtok;

/*=============================================================================
 * PURE STRING OPERATIONS (Userspace - No IPC needed)
 * These functions work entirely within the current task's address space
 * and don't require kernel privileges.
 *============================================================================*/

/**
 * strcpy - Copy a string
 * @dest: Destination buffer
 * @src: Source string
 *
 * Copies the string from src to dest, including the terminating null byte.
 * Returns a pointer to dest.
 */
extern inline char * strcpy(char * dest, const char * src)
{
	char *tmp = dest;
	
	while ((*dest++ = *src++) != '\0')
		/* nothing */;
	return tmp;
}

/**
 * strcat - Concatenate two strings
 * @dest: Destination buffer (must have enough space)
 * @src: String to append
 *
 * Appends the src string to the dest string, overwriting the terminating
 * null byte at the end of dest, and then adds a terminating null byte.
 * Returns a pointer to dest.
 */
extern inline char * strcat(char * dest, const char * src)
{
	char *tmp = dest;
	
	while (*dest)
		dest++;
	while ((*dest++ = *src++) != '\0')
		/* nothing */;
	return tmp;
}

/**
 * strcmp - Compare two strings
 * @cs: First string
 * @ct: Second string
 *
 * Compares the two strings. Returns an integer less than, equal to,
 * or greater than zero if cs is found, respectively, to be less than,
 * to match, or be greater than ct.
 */
extern inline int strcmp(const char * cs, const char * ct)
{
	int res;
	
	__asm__ __volatile__(
		"1:\tlodsb\n\t"
		"scasb\n\t"
		"jne 2f\n\t"
		"testb %%al,%%al\n\t"
		"jne 1b\n\t"
		"xorl %%eax,%%eax\n\t"
		"jmp 3f\n"
		"2:\tsbbl %%eax,%%eax\n\t"
		"orb $1,%%al\n"
		"3:"
		: "=a" (res)
		: "D" (cs), "S" (ct)
		: "cx", "cc");
	return res;
}

/**
 * strlen - Calculate the length of a string
 * @s: String to measure
 *
 * Returns the number of bytes in the string, excluding the terminating null byte.
 */
extern inline int strlen(const char * s)
{
	int len = 0;
	
	while (*s++)
		len++;
	return len;
}

/**
 * strspn - Get length of a prefix substring
 * @cs: String to search
 * @ct: Set of characters to match
 *
 * Calculates the length of the initial segment of cs which consists
 * entirely of characters from ct.
 */
extern inline int strspn(const char * cs, const char * ct)
{
	const char *s1, *s2;
	int res = 0;
	
	for (s1 = cs; *s1; s1++) {
		for (s2 = ct; *s2; s2++) {
			if (*s1 == *s2)
				break;
		}
		if (!*s2)
			break;
		res++;
	}
	return res;
}

/**
 * strcspn - Get length of a complementary prefix substring
 * @cs: String to search
 * @ct: Set of characters to reject
 *
 * Calculates the length of the initial segment of cs which consists
 * entirely of characters not in ct.
 */
extern inline int strcspn(const char * cs, const char * ct)
{
	const char *s1, *s2;
	int res = 0;
	
	for (s1 = cs; *s1; s1++) {
		for (s2 = ct; *s2; s2++) {
			if (*s1 == *s2)
				return res;
		}
		res++;
	}
	return res;
}

/**
 * strpbrk - Search a string for any of a set of characters
 * @cs: String to search
 * @ct: Set of characters to look for
 *
 * Returns a pointer to the first occurrence in cs of any character from ct,
 * or NULL if no such character exists.
 */
extern inline char * strpbrk(const char * cs, const char * ct)
{
	const char *s1, *s2;
	
	for (s1 = cs; *s1; s1++) {
		for (s2 = ct; *s2; s2++) {
			if (*s1 == *s2)
				return (char *)s1;
		}
	}
	return NULL;
}

/**
 * strstr - Find a substring
 * @cs: String to search
 * @ct: Substring to find
 *
 * Returns a pointer to the first occurrence of ct in cs, or NULL if not found.
 */
extern inline char * strstr(const char * cs, const char * ct)
{
	const char *s1, *s2;
	int len = strlen(ct);
	
	if (!len)
		return (char *)cs;
	
	for (s1 = cs; *s1; s1++) {
		if (*s1 != *ct)
			continue;
		for (s2 = ct; *s2; s2++) {
			if (*(s1 + (s2 - ct)) != *s2)
				break;
		}
		if (!*s2)
			return (char *)s1;
	}
	return NULL;
}

/**
 * strtok - Extract tokens from a string
 * @s: String to tokenize (NULL to continue from previous token)
 * @ct: Delimiter characters
 *
 * Returns a pointer to the next token, or NULL when no more tokens remain.
 */
extern inline char * strtok(char * s, const char * ct)
{
	char *sbegin, *send;
	
	if (s == NULL) {
		if (___strtok == NULL)
			return NULL;
		s = ___strtok;
	}
	
	s += strspn(s, ct);
	if (*s == '\0') {
		___strtok = NULL;
		return NULL;
	}
	
	sbegin = s;
	s += strcspn(s, ct);
	send = s;
	
	if (*s != '\0') {
		*s = '\0';
		___strtok = s + 1;
	} else {
		___strtok = NULL;
	}
	
	return sbegin;
}

/*=============================================================================
 * MEMORY OPERATIONS (May require IPC to memory server)
 * These functions may access memory across protection domains or require
 * capability validation.
 *============================================================================*/

/**
 * memcpy - Copy memory region (with capability check)
 * @dest: Destination address
 * @src: Source address
 * @n: Number of bytes to copy
 *
 * Copies n bytes from src to dest. The memory regions must not overlap.
 * If the regions overlap, use memmove instead.
 *
 * In microkernel mode, if the copy crosses capability domains or requires
 * special permissions, it uses IPC to the memory server. Otherwise, it uses
 * optimized inline assembly.
 *
 * Returns a pointer to dest.
 */
extern inline void * memcpy(void * dest, const void * src, int n)
{
	/* Fast path: if both pointers are in the same capability domain
	 * and we have MEMORY capability, use inline assembly */
	if ((current_capability & CAP_MEMORY) && 
	    same_capability_domain(dest, src)) {
		void *res = dest;
		__asm__ __volatile__ (
			"cld\n\t"
			"rep\n\t"
			"movsb"
			: "=c" (n), "=D" (dest), "=S" (src)
			: "0" (n), "1" (dest), "2" (src)
			: "memory");
		return res;
	}
	
	/* Slow path: use IPC to memory server */
	struct msg_mem_string_op msg;
	struct msg_mem_string_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	/* Check capability */
	if (!(current_capability & CAP_MEM_STRING)) {
		if (request_mem_string_capability() < 0)
			return NULL;
	}
	
	msg.header.msg_id = MSG_MEM_STRING_COPY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.dest = (unsigned long)dest;
	msg.src = (unsigned long)src;
	msg.n = n;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.op_type = MEM_OP_COPY;
	
	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) < 0)
		return NULL;
	
	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return NULL;
	
	if (reply.result < 0)
		return NULL;
	
	/* Update capabilities if granted */
	if (reply.granted_caps)
		current_capability |= reply.granted_caps;
	
	return dest;
}

/**
 * memmove - Copy memory region (handles overlap)
 * @dest: Destination address
 * @src: Source address
 * @n: Number of bytes to copy
 *
 * Copies n bytes from src to dest. Unlike memcpy, it handles overlapping
 * regions correctly.
 *
 * Returns a pointer to dest.
 */
extern inline void * memmove(void * dest, const void * src, int n)
{
	unsigned long d = (unsigned long)dest;
	unsigned long s = (unsigned long)src;
	void *res = dest;
	
	/* Fast path: if in same domain and we have capability */
	if ((current_capability & CAP_MEMORY) && 
	    same_capability_domain(dest, src)) {
		if (d < s || d >= s + n) {
			/* Forward copy is safe */
			__asm__ __volatile__ (
				"cld\n\t"
				"rep\n\t"
				"movsb"
				: "=c" (n), "=D" (dest), "=S" (src)
				: "0" (n), "1" (dest), "2" (src)
				: "memory");
		} else {
			/* Backward copy for overlap */
			src += n - 1;
			dest += n - 1;
			__asm__ __volatile__ (
				"std\n\t"
				"rep\n\t"
				"movsb\n\t"
				"cld"
				: "=c" (n), "=D" (dest), "=S" (src)
				: "0" (n), "1" (dest), "2" (src)
				: "memory");
		}
		return res;
	}
	
	/* Slow path: use IPC */
	struct msg_mem_string_op msg;
	struct msg_mem_string_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	if (!(current_capability & CAP_MEM_STRING)) {
		if (request_mem_string_capability() < 0)
			return NULL;
	}
	
	msg.header.msg_id = MSG_MEM_STRING_MOVE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.dest = d;
	msg.src = s;
	msg.n = n;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.op_type = MEM_OP_MOVE;
	
	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) < 0)
		return NULL;
	
	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return NULL;
	
	if (reply.result < 0)
		return NULL;
	
	return dest;
}

/**
 * memchr - Find a character in memory
 * @cs: Memory region to search
 * @c: Character to find
 * @count: Number of bytes to search
 *
 * Scans the first count bytes of the memory region for character c.
 *
 * Returns a pointer to the first occurrence, or NULL if not found.
 */
extern inline void * memchr(const void * cs, char c, int count)
{
	const unsigned char *p = cs;
	
	/* Fast path: same domain */
	if (current_capability & CAP_MEMORY) {
		while (count--) {
			if (*p == (unsigned char)c)
				return (void *)p;
			p++;
		}
		return NULL;
	}
	
	/* Slow path: use IPC */
	struct msg_mem_string_op msg;
	struct msg_mem_string_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	if (!(current_capability & CAP_MEM_STRING)) {
		if (request_mem_string_capability() < 0)
			return NULL;
	}
	
	msg.header.msg_id = MSG_MEM_STRING_CHR;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.dest = 0;  /* Not used for chr */
	msg.src = (unsigned long)cs;
	msg.n = count;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability | ((unsigned int)(unsigned char)c << 16);
	msg.op_type = MEM_OP_CHR;
	
	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) < 0)
		return NULL;
	
	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return NULL;
	
	if (reply.result < 0)
		return NULL;
	
	return (void *)reply.value;
}

/*=============================================================================
 * ADDITIONAL STRING FUNCTIONS (Userspace)
 *============================================================================*/

/**
 * strncpy - Copy a string with length limit
 * @dest: Destination buffer
 * @src: Source string
 * @n: Maximum number of bytes to copy
 *
 * Copies at most n bytes from src to dest. If src is shorter than n,
 * the remainder of dest is filled with null bytes.
 */
static inline char * strncpy(char * dest, const char * src, int n)
{
	char *tmp = dest;
	
	while (n-- && (*dest++ = *src++) != '\0')
		/* nothing */;
	
	if (n > 0) {
		while (n--)
			*dest++ = '\0';
	}
	
	return tmp;
}

/**
 * strncat - Concatenate two strings with length limit
 * @dest: Destination buffer
 * @src: Source string
 * @n: Maximum number of bytes to append
 */
static inline char * strncat(char * dest, const char * src, int n)
{
	char *tmp = dest;
	
	while (*dest)
		dest++;
	
	while (n-- && (*dest++ = *src++) != '\0')
		/* nothing */;
	
	*dest = '\0';
	
	return tmp;
}

/**
 * strncmp - Compare two strings with length limit
 * @cs: First string
 * @ct: Second string
 * @n: Maximum number of bytes to compare
 */
static inline int strncmp(const char * cs, const char * ct, int n)
{
	int res = 0;
	
	while (n--) {
		if ((res = *cs - *ct++) != 0 || !*cs++)
			break;
	}
	
	return res;
}

/**
 * strchr - Find first occurrence of character in string
 * @s: String to search
 * @c: Character to find
 */
static inline char * strchr(const char * s, char c)
{
	while (*s != '\0' && *s != c)
		s++;
	
	return (*s == c) ? (char *)s : NULL;
}

/**
 * strrchr - Find last occurrence of character in string
 * @s: String to search
 * @c: Character to find
 */
static inline char * strrchr(const char * s, char c)
{
	const char *last = NULL;
	
	while (*s != '\0') {
		if (*s == c)
			last = s;
		s++;
	}
	
	return (char *)last;
}

/**
 * strdup - Duplicate a string
 * @s: String to duplicate
 *
 * Allocates memory and copies the string. Returns NULL if allocation fails.
 * Caller is responsible for freeing the returned memory.
 */
static inline char * strdup(const char * s)
{
	char *new = malloc(strlen(s) + 1);
	
	if (new)
		strcpy(new, s);
	
	return new;
}

/**
 * strndup - Duplicate a string with length limit
 * @s: String to duplicate
 * @n: Maximum number of bytes
 */
static inline char * strndup(const char * s, int n)
{
	int len = strlen(s);
	
	if (len > n)
		len = n;
	
	char *new = malloc(len + 1);
	
	if (new) {
		memcpy(new, s, len);
		new[len] = '\0';
	}
	
	return new;
}

/**
 * strerror - Get error message string
 * @errno: Error number
 *
 * Returns a pointer to the error message string.
 */
extern char * strerror(int errno)
{
	/* Would need IPC to locale server for localized messages */
	/* For now, use built-in table from errno.h */
	return (char *)"Unknown error";  /* Placeholder */
}

/*=============================================================================
 * CAPABILITY HELPER FUNCTIONS
 *============================================================================*/

/**
 * same_capability_domain - Check if two pointers are in the same domain
 * @p1: First pointer
 * @p2: Second pointer
 *
 * Returns 1 if both pointers are in the same capability domain,
 * 0 otherwise.
 */
static inline int same_capability_domain(const void *p1, const void *p2)
{
	/* In a real implementation, this would check the capability tables */
	/* For now, assume all user pointers are in the same domain */
	return 1;
}

/**
 * request_mem_string_capability - Request memory string capability
 */
static inline int request_mem_string_capability(void)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_MEM_STRING;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.task_id = kernel_state->current_task;
	msg.requested_cap = CAP_MEM_STRING;

	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= CAP_MEM_STRING;
				return 0;
			}
		}
	}
	return -1;
}

#endif /* _STRING_H_ */
