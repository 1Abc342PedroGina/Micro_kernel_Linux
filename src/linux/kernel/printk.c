/*
* HISTORY
* $Log: printk.c,v $
* Revision 1.1 2026/02/15 01:15:30 pedro
* Microkernel version of kernel print function.
* printk now sends messages to console and log servers via IPC.
* Original vsprintf preserved for formatting.
* Added buffering and log levels.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/printk.c
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Kernel print function for microkernel architecture.
 *
 * Original Linux 0.11: Direct call to tty_write with assembly.
 * Microkernel version: printk sends IPC messages to console server
 * and optionally to log server. The formatting is still done locally,
 * but output is delegated to servers for security and isolation.
 *
 * Security: Console output requires CAP_CONSOLE capability.
 * Log output requires CAP_LOG capability.
 */

#include <stdarg.h>
#include <stddef.h>
#include <linux/kernel.h>
#include <linux/head.h>

/*=============================================================================
 * BUFFERS AND FORMATTING
 *============================================================================*/

static char buf[1024];		/* Formatting buffer */
static char log_buf[4096];	/* Circular log buffer */
static unsigned long log_start = 0;	/* Start of circular buffer */
static unsigned long log_end = 0;	/* End of circular buffer */
static unsigned long log_chars = 0;	/* Total chars printed */

extern int vsprintf(char * buf, const char * fmt, va_list args);

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_CONSOLE_WRITE	0x0200	/* Write to console */
#define MSG_LOG_WRITE		0x0201	/* Write to log */
#define MSG_LOG_READ		0x0202	/* Read from log */
#define MSG_LOG_CLEAR		0x0203	/* Clear log */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_console_write {
	struct mk_msg_header header;
	char data[1024];		/* Data to write */
	unsigned int len;		/* Length of data */
	unsigned int level;		/* Log level (0-7) */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_log_write {
	struct mk_msg_header header;
	char data[1024];		/* Data to log */
	unsigned int len;		/* Length of data */
	unsigned int level;		/* Log level */
	unsigned long timestamp;	/* Kernel timestamp */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_log_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	unsigned long chars;		/* Total chars logged */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_CONSOLE		0x0010	/* Can write to console */
#define CAP_LOG			0x0020	/* Can write to log */

/*=============================================================================
 * LOG LEVELS
 *============================================================================*/

#define KERN_EMERG	0	/* System is unusable */
#define KERN_ALERT	1	/* Action must be taken immediately */
#define KERN_CRIT	2	/* Critical conditions */
#define KERN_ERR	3	/* Error conditions */
#define KERN_WARNING	4	/* Warning conditions */
#define KERN_NOTICE	5	/* Normal but significant condition */
#define KERN_INFO	6	/* Informational */
#define KERN_DEBUG	7	/* Debug-level messages */

/* Default log level */
#define DEFAULT_LOG_LEVEL KERN_INFO

/* Current console log level (messages above this go to console) */
static int console_loglevel = 7;	/* Print everything */
static int default_loglevel = 7;	/* Default for messages without level */

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * printk_parse_level - Parse log level from format string
 * @fmt: Format string
 * @level: Output log level
 * 
 * Returns pointer to format after level, or original fmt.
 */
static const char *printk_parse_level(const char *fmt, int *level)
{
	*level = default_loglevel;
	
	/* Check for <level> prefix */
	if (fmt[0] == '<' && fmt[2] == '>') {
		if (fmt[1] >= '0' && fmt[1] <= '7') {
			*level = fmt[1] - '0';
			return fmt + 3;
		}
	}
	return fmt;
}

/**
 * printk_store_log - Store message in circular log buffer
 * @msg: Message to store
 * @len: Length of message
 * @level: Log level
 */
static void printk_store_log(const char *msg, int len, int level)
{
	int i;
	
	/* Add to circular buffer */
	for (i = 0; i < len; i++) {
		log_buf[log_end] = msg[i];
		log_end = (log_end + 1) & (sizeof(log_buf) - 1);
		if (log_end == log_start)
			log_start = (log_start + 1) & (sizeof(log_buf) - 1);
	}
	
	/* Add newline if missing */
	if (len > 0 && msg[len-1] != '\n') {
		log_buf[log_end] = '\n';
		log_end = (log_end + 1) & (sizeof(log_buf) - 1);
		if (log_end == log_start)
			log_start = (log_start + 1) & (sizeof(log_buf) - 1);
	}
	
	log_chars += len;
}

/*=============================================================================
 * MAIN PRINTK FUNCTION
 *============================================================================*/

/**
 * printk - Kernel print function
 * @fmt: Format string
 * @...: Arguments
 * 
 * Formats a message and sends it to console and log servers.
 * Returns number of characters printed.
 */
int printk(const char *fmt, ...)
{
	va_list args;
	int i;
	int level;
	struct msg_console_write msg_console;
	struct msg_log_write msg_log;
	int result = 0;

	/* Parse log level from format */
	fmt = printk_parse_level(fmt, &level);

	/* Format message */
	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	/* Store in local circular buffer */
	printk_store_log(buf, i, level);

	/* Send to log server (always) */
	if (kernel_state->log_server && (current_capability & CAP_LOG)) {
		msg_log.header.msg_id = MSG_LOG_WRITE;
		msg_log.header.sender_port = kernel_state->kernel_port;
		msg_log.header.reply_port = 0;  /* Fire and forget */
		msg_log.header.size = sizeof(msg_log);
		
		memcpy(msg_log.data, buf, i);
		msg_log.len = i;
		msg_log.level = level;
		msg_log.timestamp = jiffies;
		msg_log.task_id = kernel_state->current_task;
		msg_log.caps = current_capability;
		
		mk_msg_send(kernel_state->log_server, &msg_log, sizeof(msg_log));
	}

	/* Send to console server if level is high enough */
	if (level <= console_loglevel) {
		if (kernel_state->console_server && (current_capability & CAP_CONSOLE)) {
			msg_console.header.msg_id = MSG_CONSOLE_WRITE;
			msg_console.header.sender_port = kernel_state->kernel_port;
			msg_console.header.reply_port = 0;
			msg_console.header.size = sizeof(msg_console);
			
			memcpy(msg_console.data, buf, i);
			msg_console.len = i;
			msg_console.level = level;
			msg_console.task_id = kernel_state->current_task;
			msg_console.caps = current_capability;
			
			mk_msg_send(kernel_state->console_server, &msg_console, sizeof(msg_console));
		} else {
			/* Emergency fallback - direct tty_write if no server */
			__asm__ __volatile__(
				"push %%fs\n\t"
				"push %%ds\n\t"
				"pop %%fs\n\t"
				"pushl %0\n\t"
				"pushl $buf\n\t"
				"pushl $0\n\t"
				"call tty_write\n\t"
				"addl $8,%%esp\n\t"
				"popl %0\n\t"
				"pop %%fs"
				: : "r" (i) : "ax", "cx", "dx");
		}
	}

	return i;
}

/*=============================================================================
 * LOG MANAGEMENT FUNCTIONS
 *============================================================================*/

/**
 * printk_get_log - Read from kernel log buffer
 * @buffer: Destination buffer
 * @len: Buffer length
 * 
 * Returns number of bytes read.
 */
int printk_get_log(char *buffer, int len)
{
	unsigned long i = 0;
	unsigned long pos = log_start;
	
	if (!buffer || len <= 0)
		return 0;
	
	/* Copy from circular buffer */
	while (i < len - 1 && pos != log_end) {
		buffer[i++] = log_buf[pos];
		pos = (pos + 1) & (sizeof(log_buf) - 1);
	}
	
	buffer[i] = '\0';
	return i;
}

/**
 * printk_clear_log - Clear kernel log buffer
 */
void printk_clear_log(void)
{
	log_start = 0;
	log_end = 0;
	log_chars = 0;
}

/**
 * printk_set_loglevel - Set console log level
 * @level: New log level (0-7)
 * 
 * Returns old log level.
 */
int printk_set_loglevel(int level)
{
	int old = console_loglevel;
	
	if (level >= 0 && level <= 7)
		console_loglevel = level;
	
	return old;
}

/*=============================================================================
 * SPECIALIZED PRINT FUNCTIONS
 *============================================================================*/

/**
 * printk_emerg - Print emergency message
 */
int printk_emerg(const char *fmt, ...)
{
	va_list args;
	char local_buf[1024];
	int i;
	
	va_start(args, fmt);
	i = vsprintf(local_buf, fmt, args);
	va_end(args);
	
	return printk("<0>%s", local_buf);
}

/**
 * printk_alert - Print alert message
 */
int printk_alert(const char *fmt, ...)
{
	va_list args;
	char local_buf[1024];
	int i;
	
	va_start(args, fmt);
	i = vsprintf(local_buf, fmt, args);
	va_end(args);
	
	return printk("<1>%s", local_buf);
}

/**
 * printk_crit - Print critical message
 */
int printk_crit(const char *fmt, ...)
{
	va_list args;
	char local_buf[1024];
	int i;
	
	va_start(args, fmt);
	i = vsprintf(local_buf, fmt, args);
	va_end(args);
	
	return printk("<2>%s", local_buf);
}

/**
 * printk_err - Print error message
 */
int printk_err(const char *fmt, ...)
{
	va_list args;
	char local_buf[1024];
	int i;
	
	va_start(args, fmt);
	i = vsprintf(local_buf, fmt, args);
	va_end(args);
	
	return printk("<3>%s", local_buf);
}

/**
 * printk_warning - Print warning message
 */
int printk_warning(const char *fmt, ...)
{
	va_list args;
	char local_buf[1024];
	int i;
	
	va_start(args, fmt);
	i = vsprintf(local_buf, fmt, args);
	va_end(args);
	
	return printk("<4>%s", local_buf);
}

/**
 * printk_notice - Print notice message
 */
int printk_notice(const char *fmt, ...)
{
	va_list args;
	char local_buf[1024];
	int i;
	
	va_start(args, fmt);
	i = vsprintf(local_buf, fmt, args);
	va_end(args);
	
	return printk("<5>%s", local_buf);
}

/**
 * printk_info - Print informational message
 */
int printk_info(const char *fmt, ...)
{
	va_list args;
	char local_buf[1024];
	int i;
	
	va_start(args, fmt);
	i = vsprintf(local_buf, fmt, args);
	va_end(args);
	
	return printk("<6>%s", local_buf);
}

/**
 * printk_debug - Print debug message
 */
int printk_debug(const char *fmt, ...)
{
	va_list args;
	char local_buf[1024];
	int i;
	
	va_start(args, fmt);
	i = vsprintf(local_buf, fmt, args);
	va_end(args);
	
	return printk("<7>%s", local_buf);
}

/*=============================================================================
 * EARLY PRINTK (before servers are ready)
 *============================================================================*/

/**
 * early_printk - Print before servers are initialized
 * @fmt: Format string
 * @...: Arguments
 * 
 * Uses direct tty_write when servers aren't available.
 */
int early_printk(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	/* Direct tty_write (original method) */
	__asm__ __volatile__(
		"push %%fs\n\t"
		"push %%ds\n\t"
		"pop %%fs\n\t"
		"pushl %0\n\t"
		"pushl $buf\n\t"
		"pushl $0\n\t"
		"call tty_write\n\t"
		"addl $8,%%esp\n\t"
		"popl %0\n\t"
		"pop %%fs"
		: : "r" (i) : "ax", "cx", "dx");

	return i;
}
