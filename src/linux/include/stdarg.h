/*
* HISTORY
* $Log: stdarg.h,v $
* Revision 1.1 2026/02/14 11:15:30 pedro
* Microkernel version of variable argument handling.
* Preserved original macros for compatibility.
* Added capability-aware argument passing for IPC.
* Extended with server-side argument marshaling.
* [2026/02/14 pedro]
*/

/*
* File: stdarg.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Variable argument handling for microkernel architecture.
*
* Original Linux 0.11: Standard variable argument macros for C.
* Microkernel version: Same macros preserved for compatibility, but now
* used extensively in IPC message construction. The va_list type is used
* to marshal arguments for system call stubs and server handlers.
*
* Note: This header is pure userspace - no IPC needed as argument handling
* is done at compile time and in userspace stack manipulation.
*/

#ifndef _STDARG_H
#define _STDARG_H

/*=============================================================================
 * ORIGINAL TYPE DEFINITIONS (Preserved for compatibility)
 *============================================================================*/

/**
 * va_list - Variable argument list type
 * 
 * In microkernel mode, va_list is still a pointer to arguments on the stack,
 * but it's also used to marshal arguments for IPC messages to servers.
 */
typedef char *va_list;

/*=============================================================================
 * ORIGINAL MACROS (Preserved exactly)
 *============================================================================*/

/* Amount of space required in an argument list for an arg of type TYPE.
   TYPE may alternatively be an expression whose type is used. */
#define __va_rounded_size(TYPE)  \
  (((sizeof (TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

#ifndef __sparc__
#define va_start(AP, LASTARG) 						\
 (AP = ((char *) &(LASTARG) + __va_rounded_size (LASTARG)))
#else
#define va_start(AP, LASTARG) 						\
 (__builtin_saveregs (),						\
  AP = ((char *) &(LASTARG) + __va_rounded_size (LASTARG)))
#endif

/*
 * va_end - Clean up variable arguments
 * In original: defined in gnulib
 * In microkernel: still a no-op for normal usage, but used for IPC cleanup
 */
void va_end (va_list);
#define va_end(AP)

#define va_arg(AP, TYPE)						\
 (AP += __va_rounded_size (TYPE),					\
  *((TYPE *) (AP - __va_rounded_size (TYPE))))

/*=============================================================================
 * MICROKERNEL EXTENSIONS FOR IPC ARGUMENT MARSHALING
 *============================================================================*/

/**
 * Maximum number of arguments that can be marshaled for IPC
 * Based on syscall argument limit (3 direct + 6 in message = 9 total)
 */
#define MAX_IPC_ARGS 9

/**
 * IPC argument types for marshaling
 */
#define IPC_ARG_NONE    0
#define IPC_ARG_INT     1
#define IPC_ARG_LONG    2
#define IPC_ARG_PTR     3
#define IPC_ARG_STR     4
#define IPC_ARG_STRUCT  5

/**
 * ipc_arg_t - Type for marshaled IPC argument
 */
typedef struct {
	unsigned int type;		/* Argument type (IPC_ARG_*) */
	unsigned long value;		/* Actual value */
	unsigned int size;		/* Size (for structs/strings) */
	capability_t caps;		/* Required capabilities */
} ipc_arg_t;

/**
 * va_to_ipc_args - Convert va_list to IPC argument array
 * @ap: Variable argument list
 * @arg_count: Number of arguments to extract
 * @args: Output array of ipc_arg_t
 * 
 * Converts variable arguments into a format suitable for IPC messages.
 * Used by syscall stubs to marshal arguments for servers.
 * 
 * Returns 0 on success, -1 on error.
 */
static inline int va_to_ipc_args(va_list ap, int arg_count, ipc_arg_t *args)
{
	int i;
	
	if (arg_count > MAX_IPC_ARGS)
		return -1;  /* EINVAL */
	
	for (i = 0; i < arg_count; i++) {
		/* Extract next argument */
		void *ptr = va_arg(ap, void *);
		
		/* Try to determine type (simplified - in practice, type info needed) */
		args[i].type = IPC_ARG_LONG;
		args[i].value = (unsigned long)ptr;
		args[i].size = sizeof(unsigned long);
		args[i].caps = current_capability;
	}
	
	return 0;
}

/**
 * ipc_args_to_va - Convert IPC arguments back to va_list
 * @args: IPC argument array
 * @arg_count: Number of arguments
 * @ap: Destination va_list (must be initialized)
 * 
 * Used by servers to unpack IPC arguments.
 */
static inline void ipc_args_to_va(ipc_arg_t *args, int arg_count, va_list ap)
{
	int i;
	
	for (i = 0; i < arg_count; i++) {
		/* In practice, would need to push onto stack appropriately */
		/* This is simplified for illustration */
	}
}

/*=============================================================================
 * CAPABILITY-AWARE ARGUMENT VALIDATION
 *============================================================================*/

/**
 * va_validate_caps - Validate capabilities for variable arguments
 * @ap: Variable argument list
 * @arg_count: Number of arguments
 * @required_caps: Required capabilities for each argument (bitmask)
 * 
 * Checks that the caller has the necessary capabilities for each argument.
 * Used in server stubs before processing requests.
 * 
 * Returns 1 if all capabilities are satisfied, 0 otherwise.
 */
static inline int va_validate_caps(va_list ap, int arg_count, 
                                    unsigned int required_caps)
{
	int i;
	void *arg;
	
	for (i = 0; i < arg_count; i++) {
		arg = va_arg(ap, void *);
		
		/* Check if argument requires special capability */
		if (required_caps & (1 << i)) {
			/* Would call capability server to validate */
			if (!(current_capability & CAP_MEMORY))
				return 0;
		}
	}
	
	return 1;
}

/*=============================================================================
 * SERVER-SIDE ARGUMENT HANDLING
 *============================================================================*/

/**
 * va_server_start - Start processing variable arguments in server
 * @ap: va_list to initialize
 * @lastarg: Last fixed argument
 * @msg: IPC message containing arguments
 * 
 * Special version for servers receiving IPC messages.
 */
#define va_server_start(ap, lastarg, msg) \
	va_start(ap, lastarg)

/**
 * va_server_arg - Extract argument from IPC message with capability check
 * @ap: va_list
 * @type: Expected type
 * @cap_required: Capability required for this argument
 * 
 * Returns argument value if capability check passes, 0 otherwise.
 */
#define va_server_arg(ap, type, cap_required) \
	({ \
		type _val = va_arg(ap, type); \
		if ((cap_required) && !(current_capability & (cap_required))) \
			_val = (type)0; \
		_val; \
	})

/*=============================================================================
 * IPC MESSAGE CONSTRUCTION UTILITIES
 *============================================================================*/

/**
 * struct ipc_msg_template - Template for building IPC messages
 */
struct ipc_msg_template {
	unsigned int msg_id;		/* Message ID */
	unsigned int arg_count;		/* Number of arguments */
	ipc_arg_t args[MAX_IPC_ARGS];	/* Argument templates */
	capability_t required_caps;	/* Required capabilities */
};

/**
 * va_build_ipc_message - Build IPC message from variable arguments
 * @template: Message template
 * @ap: Variable argument list
 * @buffer: Output buffer for message
 * @buffer_size: Size of output buffer
 * 
 * Constructs an IPC message from variable arguments using a template.
 * Used by system call stubs.
 * 
 * Returns size of constructed message, or -1 on error.
 */
static inline int va_build_ipc_message(struct ipc_msg_template *template,
                                        va_list ap, void *buffer, int buffer_size)
{
	struct mk_msg_header *header = (struct mk_msg_header *)buffer;
	ipc_arg_t *args = (ipc_arg_t *)(header + 1);
	int i;
	
	if (buffer_size < sizeof(struct mk_msg_header) + 
	                  template->arg_count * sizeof(ipc_arg_t))
		return -1;
	
	/* Fill header */
	header->msg_id = template->msg_id;
	header->sender_port = kernel_state->kernel_port;
	header->reply_port = kernel_state->kernel_port;
	header->size = sizeof(struct mk_msg_header) + 
	               template->arg_count * sizeof(ipc_arg_t);
	
	/* Fill arguments */
	for (i = 0; i < template->arg_count; i++) {
		args[i] = template->args[i];
		
		/* Override value from va_list if template value is 0 */
		if (args[i].value == 0) {
			args[i].value = (unsigned long)va_arg(ap, void *);
		}
	}
	
	return header->size;
}

/*=============================================================================
 * DEBUG AND TRACING
 *============================================================================*/

/**
 * va_dump - Dump variable argument list (for debugging)
 * @ap: va_list to dump
 * @count: Number of arguments to dump
 * 
 * Prints argument values (only available when compiled with DEBUG).
 */
#ifdef DEBUG
static inline void va_dump(va_list ap, int count)
{
	int i;
	
	printf("va_list dump (%d arguments):\n", count);
	for (i = 0; i < count; i++) {
		void *val = va_arg(ap, void *);
		printf("  arg%d: %p\n", i, val);
	}
}
#else
#define va_dump(ap, count) /* nothing */
#endif

/*=============================================================================
 * COMPATIBILITY WITH EXISTING CODE
 *============================================================================*/

/* Ensure original macros work exactly as before */
#ifndef __GNUC__
#error "This header requires GNU C extensions"
#endif

/* va_copy macro (C99) - for copying va_list */
#ifdef __GNUC__
#define va_copy(dest, src) __builtin_va_copy(dest, src)
#endif

/*=============================================================================
 * ARCHITECTURE-SPECIFIC ADJUSTMENTS
 *============================================================================*/

#ifdef __i386__
/* i386-specific argument alignment */
#undef __va_rounded_size
#define __va_rounded_size(TYPE) \
	(((sizeof(TYPE) + sizeof(int) - 1) & ~(sizeof(int) - 1)))
#endif

#ifdef __x86_64__
/* x86_64 uses different ABI - would need adjustments */
#error "x86_64 not yet supported in this microkernel version"
#endif

#endif /* _STDARG_H */
