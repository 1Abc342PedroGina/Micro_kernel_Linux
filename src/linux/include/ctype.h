/*
* HISTORY
* $Log: ctype.h,v $
* Revision 1.1 2026/02/14 08:15:30 pedro
* Microkernel version of character type functions.
* Pure userspace implementation - no IPC needed.
* Preserved original bitmask definitions for compatibility.
* Added capability-safe character classification.
* [2026/02/14 pedro]
*/

/*
* File: ctype.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Character type functions for microkernel architecture.
*
* Original Linux 0.11: Simple bitmask-based character classification.
* Microkernel version: Same implementation, but now with awareness of
* capability domains for locale support. Character classification is
* pure userspace - no IPC needed as it doesn't require kernel privileges.
*
* Note: This is one of the few headers that remains almost identical
* to the original, as character classification is stateless and
* doesn't require system calls.
*/

#ifndef _CTYPE_H
#define _CTYPE_H

/* Character property bit definitions (preserved from original) */
#define _U	0x01	/* Upper case letter */
#define _L	0x02	/* Lower case letter */
#define _D	0x04	/* Digit */
#define _C	0x08	/* Control character */
#define _P	0x10	/* Punctuation */
#define _S	0x20	/* White space (space/lf/tab) */
#define _X	0x40	/* Hexadecimal digit */
#define _SP	0x80	/* Hard space (0x20) */

/* Capability domain flags (extensions) */
#define _CAP_DOMAIN_1	0x0100	/* Capability domain 1 specific */
#define _CAP_DOMAIN_2	0x0200	/* Capability domain 2 specific */
#define _CAP_DOMAIN_3	0x0400	/* Capability domain 3 specific */
#define _CAP_LOCALE	0x0800	/* Locale-specific classification */

/* Character type table - same as original but can be extended per domain */
extern unsigned char _ctype[];
extern char _ctmp;

/* Per-capability domain locale data */
struct ctype_locale {
	unsigned char ctype_table[256];	/* Character types for this domain */
	unsigned char toupper_table[256];	/* Upper case mapping */
	unsigned char tolower_table[256];	/* Lower case mapping */
	unsigned int domain_id;			/* Capability domain ID */
};

/* Current locale for this capability domain */
extern struct ctype_locale *_current_locale;

/* Standard POSIX character classification macros (preserved) */

/**
 * isalnum - Check if character is alphanumeric
 * @c: Character to check (converted to unsigned char)
 * 
 * Returns non-zero if c is a letter or digit.
 */
#define isalnum(c) ((_ctype+1)[(unsigned char)(c)] & (_U|_L|_D))

/**
 * isalpha - Check if character is alphabetic
 * @c: Character to check
 * 
 * Returns non-zero if c is a letter.
 */
#define isalpha(c) ((_ctype+1)[(unsigned char)(c)] & (_U|_L))

/**
 * iscntrl - Check if character is a control character
 * @c: Character to check
 * 
 * Returns non-zero if c is a control character (0x00-0x1F or 0x7F).
 */
#define iscntrl(c) ((_ctype+1)[(unsigned char)(c)] & (_C))

/**
 * isdigit - Check if character is a decimal digit
 * @c: Character to check
 * 
 * Returns non-zero if c is a digit (0-9).
 */
#define isdigit(c) ((_ctype+1)[(unsigned char)(c)] & (_D))

/**
 * isgraph - Check if character is printable (except space)
 * @c: Character to check
 * 
 * Returns non-zero if c is a printable character except space.
 */
#define isgraph(c) ((_ctype+1)[(unsigned char)(c)] & (_P|_U|_L|_D))

/**
 * islower - Check if character is a lowercase letter
 * @c: Character to check
 * 
 * Returns non-zero if c is a lowercase letter.
 */
#define islower(c) ((_ctype+1)[(unsigned char)(c)] & (_L))

/**
 * isprint - Check if character is printable (including space)
 * @c: Character to check
 * 
 * Returns non-zero if c is a printable character including space.
 */
#define isprint(c) ((_ctype+1)[(unsigned char)(c)] & (_P|_U|_L|_D|_SP))

/**
 * ispunct - Check if character is punctuation
 * @c: Character to check
 * 
 * Returns non-zero if c is a punctuation character.
 */
#define ispunct(c) ((_ctype+1)[(unsigned char)(c)] & (_P))

/**
 * isspace - Check if character is whitespace
 * @c: Character to check
 * 
 * Returns non-zero if c is whitespace (space, \f, \n, \r, \t, \v).
 */
#define isspace(c) ((_ctype+1)[(unsigned char)(c)] & (_S))

/**
 * isupper - Check if character is an uppercase letter
 * @c: Character to check
 * 
 * Returns non-zero if c is an uppercase letter.
 */
#define isupper(c) ((_ctype+1)[(unsigned char)(c)] & (_U))

/**
 * isxdigit - Check if character is a hexadecimal digit
 * @c: Character to check
 * 
 * Returns non-zero if c is a hexadecimal digit (0-9, a-f, A-F).
 */
#define isxdigit(c) ((_ctype+1)[(unsigned char)(c)] & (_D|_X))

/**
 * isascii - Check if character is ASCII
 * @c: Character to check
 * 
 * Returns non-zero if c is within the ASCII range (0-127).
 */
#define isascii(c) (((unsigned)(c)) <= 0x7f)

/**
 * toascii - Convert character to ASCII
 * @c: Character to convert
 * 
 * Returns c masked to 7 bits (ASCII range).
 */
#define toascii(c) (((unsigned)(c)) & 0x7f)

/**
 * tolower - Convert character to lowercase
 * @c: Character to convert
 * 
 * If c is an uppercase letter, returns the corresponding lowercase letter.
 * Otherwise returns c unchanged.
 */
#define tolower(c) (_ctmp = (c), isupper(_ctmp) ? _ctmp - ('A' - 'a') : _ctmp)

/**
 * toupper - Convert character to uppercase
 * @c: Character to convert
 * 
 * If c is a lowercase letter, returns the corresponding uppercase letter.
 * Otherwise returns c unchanged.
 */
#define toupper(c) (_ctmp = (c), islower(_ctmp) ? _ctmp - ('a' - 'A') : _ctmp)

/* Capability-aware extensions */

/**
 * isalnum_cap - Check if character is alphanumeric in current capability domain
 * @c: Character to check
 * 
 * Like isalnum but respects locale settings for the current capability domain.
 */
#define isalnum_cap(c) \
	(_current_locale ? \
	 (_current_locale->ctype_table[(unsigned char)(c)] & (_U|_L|_D)) : \
	 isalnum(c))

/**
 * isalpha_cap - Check if character is alphabetic in current capability domain
 * @c: Character to check
 */
#define isalpha_cap(c) \
	(_current_locale ? \
	 (_current_locale->ctype_table[(unsigned char)(c)] & (_U|_L)) : \
	 isalpha(c))

/**
 * tolower_cap - Convert to lowercase using domain-specific mapping
 * @c: Character to convert
 */
#define tolower_cap(c) \
	(_current_locale ? \
	 _current_locale->tolower_table[(unsigned char)(c)] : \
	 tolower(c))

/**
 * toupper_cap - Convert to uppercase using domain-specific mapping
 * @c: Character to convert
 */
#define toupper_cap(c) \
	(_current_locale ? \
	 _current_locale->toupper_table[(unsigned char)(c)] : \
	 toupper(c))

/* Locale management functions (may require IPC to locale server) */

/**
 * set_ctype_domain - Set character type table for current capability domain
 * @domain_id: Capability domain ID
 * 
 * Loads the appropriate locale data for the specified capability domain.
 * This may require IPC to the locale server if the domain has a custom locale.
 * 
 * Returns 0 on success, -1 on error.
 */
static inline int set_ctype_domain(unsigned int domain_id)
{
	/* Would need IPC to locale server for non-default domains */
	if (domain_id == 0) {
		/* Default domain - use built-in table */
		_current_locale = NULL;
		return 0;
	}
	
	/* For custom domains, would need to request locale data */
	/* This would be a message to the locale server */
	return -1;  /* ENOSYS */
}

/**
 * load_ctype_table - Load character type table from locale server
 * @locale_name: Name of locale (e.g., "en_US.UTF-8")
 * 
 * Requests locale data from the locale server. The server checks
 * if the current capability domain has access to the specified locale.
 * 
 * Returns pointer to locale data on success, NULL on error.
 */
static inline struct ctype_locale *load_ctype_table(const char *locale_name)
{
	struct msg_locale_request msg;
	struct msg_locale_reply reply;
	unsigned int reply_size = sizeof(reply);
	struct ctype_locale *locale;
	
	/* Prepare IPC message to locale server */
	msg.header.msg_id = MSG_LOCALE_GET;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.locale_name = locale_name;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	
	/* Send request */
	if (mk_msg_send(kernel_state->locale_server, &msg, sizeof(msg)) < 0)
		return NULL;
	
	/* Receive reply */
	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return NULL;
	
	if (reply.result < 0)
		return NULL;
	
	/* Allocate locale structure and copy data */
	locale = malloc(sizeof(struct ctype_locale));
	if (!locale)
		return NULL;
	
	memcpy(locale->ctype_table, reply.ctype_table, 256);
	memcpy(locale->toupper_table, reply.toupper_table, 256);
	memcpy(locale->tolower_table, reply.tolower_table, 256);
	locale->domain_id = reply.domain_id;
	
	return locale;
}

/* Character class test functions (for those who prefer functions) */

static inline int isalnum_func(int c) { return isalnum(c); }
static inline int isalpha_func(int c) { return isalpha(c); }
static inline int iscntrl_func(int c) { return iscntrl(c); }
static inline int isdigit_func(int c) { return isdigit(c); }
static inline int isgraph_func(int c) { return isgraph(c); }
static inline int islower_func(int c) { return islower(c); }
static inline int isprint_func(int c) { return isprint(c); }
static inline int ispunct_func(int c) { return ispunct(c); }
static inline int isspace_func(int c) { return isspace(c); }
static inline int isupper_func(int c) { return isupper(c); }
static inline int isxdigit_func(int c) { return isxdigit(c); }
static inline int isascii_func(int c) { return isascii(c); }
static inline int tolower_func(int c) { return tolower(c); }
static inline int toupper_func(int c) { return toupper(c); }

/* Extended character classes (for Unicode support if needed) */

#define isblank(c) ((c) == ' ' || (c) == '\t')
#define isblank_func(c) isblank(c)

#define iscntrl_ext(c) (iscntrl(c) || (c) == 0x7F)
#define isprint_ext(c) (isprint(c) || (c) >= 0xA0)

/* Character type table initialization */

/**
 * _init_ctype - Initialize character type table
 * 
 * Called during program startup to set up the default
 * character classification table.
 */
static inline void _init_ctype(void)
{
	/* The _ctype table is typically provided by libc */
	/* Just ensure _current_locale is NULL for default */
	_current_locale = NULL;
}

/* ASCII character type table - typical contents for reference */
/*
 * Standard ASCII character types:
 * 0x00-0x1F: Control characters (_C)
 * 0x20: Space (_S|_SP)
 * 0x21-0x2F: Punctuation (_P) !"#$%&'()*+,-./
 * 0x30-0x39: Digits (_D|_X) 0-9
 * 0x3A-0x40: Punctuation (_P) :;<=>?@
 * 0x41-0x46: Upper hex (_U|_X) A-F
 * 0x47-0x5A: Upper (_U) G-Z
 * 0x5B-0x60: Punctuation (_P) [\]^_`
 * 0x61-0x66: Lower hex (_L|_X) a-f
 * 0x67-0x7A: Lower (_L) g-z
 * 0x7B-0x7E: Punctuation (_P) {|}~
 * 0x7F: Control (_C) DEL
 */

#endif /* _CTYPE_H */
