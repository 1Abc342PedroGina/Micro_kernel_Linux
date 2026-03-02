/*
* HISTORY
* $Log: config.h,v $
* Revision 1.1 2026/02/14 18:30:45 pedro
* Microkernel version of system configuration.
* Original KBD_US preserved for compatibility.
* Added capability-aware configuration flags.
* Configuration now managed by system server.
* [2026/02/14 pedro]
*/

/*
* File: linux/config.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* System configuration for microkernel architecture.
*
* Original Linux 0.11: Simple compile-time configuration.
* Microkernel version: Configuration is now dynamic and capability-aware.
* Settings can be changed at runtime (with appropriate capabilities)
* and are managed by the system server.
*
* Security: Changing configuration requires CAP_SYSTEM capability.
* Different capability domains can have different configurations.
*/

#ifndef _CONFIG_H
#define _CONFIG_H

/*=============================================================================
 * ORIGINAL CONFIGURATION (Preserved for compatibility)
 *============================================================================*/

/*
 * Original keyboard configuration - still used as default
 * Keyboard layouts:
 * - KBD_US: US keyboard (default)
 * - KBD_UK: United Kingdom
 * - KBD_FR: French
 * - KBD_DE: German
 * - KBD_ES: Spanish
 * - KBD_IT: Italian
 */
#define KBD_US

/*=============================================================================
 * MICROKERNEL CONFIGURATION FLAGS
 *============================================================================*/

/*=============================================================================
 * System Configuration Flags
 *============================================================================*/

#define CONFIG_MICROKERNEL	1	/* We are running on microkernel */
#define CONFIG_CAPABILITIES	1	/* Capability system enabled */
#define CONFIG_IPC		1	/* IPC system enabled */
#define CONFIG_MULTISERVER	1	/* Multiple server support */

/*=============================================================================
 * Memory Configuration
 *============================================================================*/

#define CONFIG_PAGE_SIZE	4096	/* Page size in bytes */
#define CONFIG_MAX_TASKS	64	/* Maximum number of tasks */
#define CONFIG_MAX_PORTS	256	/* Maximum IPC ports */
#define CONFIG_MAX_CAP_SPACES	16	/* Maximum capability spaces */
#define CONFIG_MAX_SERVERS	32	/* Maximum number of servers */

/*=============================================================================
 * Device Configuration
 *============================================================================*/

#define CONFIG_MAX_DEVICES	16	/* Maximum number of devices */
#define CONFIG_MAX_DISKS	4	/* Maximum disk drives */
#define CONFIG_MAX_TTYS		8	/* Maximum terminals */
#define CONFIG_MAX_NET_IF	4	/* Maximum network interfaces */

/*=============================================================================
 * File System Configuration
 *============================================================================*/

#define CONFIG_MAX_FILES	256	/* Maximum open files per task */
#define CONFIG_MAX_FS		8	/* Maximum file systems */
#define CONFIG_BLOCK_SIZE	1024	/* File system block size */
#define CONFIG_PIPE_SIZE	4096	/* Pipe buffer size */

/*=============================================================================
 * IPC Configuration
 *============================================================================*/

#define CONFIG_IPC_MAX_MSG	4096	/* Maximum IPC message size */
#define CONFIG_IPC_MAX_QUEUE	64	/* Messages per queue */
#define CONFIG_IPC_TIMEOUT	5000	/* Default timeout (ms) */

/*=============================================================================
 * Capability Configuration
 *============================================================================*/

#define CONFIG_CAP_MAX_RIGHTS	16	/* Maximum capability rights */
#define CONFIG_CAP_INHERIT	1	/* Enable capability inheritance */
#define CONFIG_CAP_REVOKE	1	/* Enable capability revocation */

/*=============================================================================
 * Debug Configuration
 *============================================================================*/

#define CONFIG_DEBUG_IPC	0	/* Debug IPC messages */
#define CONFIG_DEBUG_CAP	0	/* Debug capability operations */
#define CONFIG_DEBUG_MEM	0	/* Debug memory operations */
#define CONFIG_DEBUG_FS		0	/* Debug file system */
#define CONFIG_DEBUG_TTY	0	/* Debug terminal I/O */

/*=============================================================================
 * Keyboard Layout Configuration (Extended)
 *============================================================================*/

/* Available keyboard layouts */
#define KBD_US		1	/* US keyboard */
#define KBD_UK		2	/* United Kingdom */
#define KBD_FR		3	/* French */
#define KBD_DE		4	/* German */
#define KBD_ES		5	/* Spanish */
#define KBD_IT		6	/* Italian */
#define KBD_JP		7	/* Japanese */
#define KBD_RU		8	/* Russian */
#define KBD_BR		9	/* Brazilian */
#define KBD_DK		10	/* Danish */
#define KBD_SE		11	/* Swedish */
#define KBD_FI		12	/* Finnish */
#define KBD_NO		13	/* Norwegian */
#define KBD_PL		14	/* Polish */
#define KBD_CZ		15	/* Czech */
#define KBD_HU		16	/* Hungarian */

/* Default keyboard layout */
#ifndef DEFAULT_KBD
#define DEFAULT_KBD	KBD_US
#endif

/*=============================================================================
 * Console Configuration
 *============================================================================*/

#define CONFIG_CONSOLE_COLS	80	/* Console columns */
#define CONFIG_CONSOLE_ROWS	25	/* Console rows */
#define CONFIG_CONSOLE_TAB	8	/* Tab stops */
#define CONFIG_CONSOLE_BLANK	600	/* Screen blank timeout (seconds) */

/*=============================================================================
 * Timer Configuration
 *============================================================================*/

#define CONFIG_HZ		100	/* Timer interrupts per second */
#define CONFIG_TIME_QUALITY	1000	/* Time server quality */

/*=============================================================================
 * Bootstrap Configuration
 *============================================================================*/

#define CONFIG_BOOT_DEVICE	0x300	/* Default boot device (hd0) */
#define CONFIG_ROOT_DEVICE	0x301	/* Default root device (hd1) */
#define CONFIG_SWAP_DEVICE	0	/* Default swap device (none) */

/*=============================================================================
 * Runtime Configuration Structure
 *============================================================================*/

/*
 * System configuration - managed by system server
 * This structure can be queried via IPC
 */
struct sys_config {
	/* Version information */
	unsigned int version_major;	/* Major version */
	unsigned int version_minor;	/* Minor version */
	unsigned int version_patch;	/* Patch level */
	
	/* System limits */
	unsigned int max_tasks;		/* Maximum tasks */
	unsigned int max_ports;		/* Maximum ports */
	unsigned int max_files;		/* Maximum open files */
	unsigned int max_cap_spaces;	/* Maximum capability spaces */
	
	/* Memory configuration */
	unsigned long total_memory;	/* Total physical memory */
	unsigned long buffer_memory;	/* Buffer cache memory */
	unsigned long reserved_memory;	/* Reserved memory */
	
	/* Device configuration */
	unsigned int num_disks;		/* Number of disks */
	unsigned int num_ttys;		/* Number of terminals */
	unsigned int num_net_if;	/* Number of network interfaces */
	
	/* Keyboard configuration */
	unsigned int kbd_layout;	/* Keyboard layout */
	
	/* Console configuration */
	unsigned int console_cols;	/* Console columns */
	unsigned int console_rows;	/* Console rows */
	
	/* Boot configuration */
	unsigned int boot_dev;		/* Boot device */
	unsigned int root_dev;		/* Root device */
	unsigned int swap_dev;		/* Swap device */
	
	/* Feature flags */
	unsigned int features;		/* Feature flags */
};

/* Feature flags */
#define SYS_FEATURE_CAPABILITIES	0x0001	/* Capabilities enabled */
#define SYS_FEATURE_IPC			0x0002	/* IPC enabled */
#define SYS_FEATURE_NETWORK		0x0004	/* Networking enabled */
#define SYS_FEATURE_SMP			0x0008	/* SMP enabled */
#define SYS_FEATURE_DEBUG		0x0010	/* Debug enabled */

/*=============================================================================
 * IPC Message Codes for Configuration
 *============================================================================*/

#define MSG_CONFIG_GET		0x1500	/* Get configuration */
#define MSG_CONFIG_SET		0x1501	/* Set configuration */
#define MSG_CONFIG_GET_KBD	0x1502	/* Get keyboard layout */
#define MSG_CONFIG_SET_KBD	0x1503	/* Set keyboard layout */
#define MSG_CONFIG_GET_LIMIT	0x1504	/* Get system limit */
#define MSG_CONFIG_SET_LIMIT	0x1505	/* Set system limit */
#define MSG_CONFIG_REPLY	0x1506	/* Reply from system server */

/*=============================================================================
 * IPC Message Structures
 *============================================================================*/

struct msg_config_get {
	struct mk_msg_header header;
	unsigned int param;		/* Configuration parameter */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_config_set {
	struct mk_msg_header header;
	unsigned int param;		/* Configuration parameter */
	unsigned long value;		/* New value */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_config_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		unsigned long value;	/* Configuration value */
		struct sys_config config;	/* Full configuration */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * Configuration Parameters
 *============================================================================*/

#define CONFIG_PARAM_KBD		1	/* Keyboard layout */
#define CONFIG_PARAM_MAX_TASKS		2	/* Maximum tasks */
#define CONFIG_PARAM_MAX_FILES		3	/* Maximum open files */
#define CONFIG_PARAM_CONSOLE_COLS	4	/* Console columns */
#define CONFIG_PARAM_CONSOLE_ROWS	5	/* Console rows */
#define CONFIG_PARAM_BOOT_DEV		6	/* Boot device */
#define CONFIG_PARAM_ROOT_DEV		7	/* Root device */
#define CONFIG_PARAM_SWAP_DEV		8	/* Swap device */
#define CONFIG_PARAM_FEATURES		9	/* Feature flags */

/*=============================================================================
 * Configuration Access Functions
 *============================================================================*/

/**
 * config_get - Get configuration parameter
 * @param: Configuration parameter (CONFIG_PARAM_*)
 * 
 * Returns parameter value, or -1 on error.
 */
static inline long config_get(unsigned int param)
{
	struct msg_config_get msg;
	struct msg_config_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	msg.header.msg_id = MSG_CONFIG_GET;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.param = param;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return (reply.result < 0) ? -1 : reply.data.value;
}

/**
 * config_set - Set configuration parameter
 * @param: Configuration parameter
 * @value: New value
 * 
 * Returns 0 on success, -1 on error.
 * Requires CAP_SYSTEM capability.
 */
static inline int config_set(unsigned int param, unsigned long value)
{
	struct msg_config_set msg;
	struct msg_config_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_SYSTEM))
		return -1;

	msg.header.msg_id = MSG_CONFIG_SET;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.param = param;
	msg.value = value;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return reply.result;
}

/**
 * config_get_system - Get full system configuration
 * @config: Output configuration structure
 * 
 * Returns 0 on success, -1 on error.
 */
static inline int config_get_system(struct sys_config *config)
{
	struct msg_config_get msg;
	struct msg_config_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!config)
		return -1;

	msg.header.msg_id = MSG_CONFIG_GET;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.param = 0;  /* Special: get full config */
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	if (reply.result == 0)
		*config = reply.data.config;

	return reply.result;
}

/*=============================================================================
 * Keyboard Configuration Functions
 *============================================================================*/

/**
 * config_get_kbd - Get current keyboard layout
 * 
 * Returns keyboard layout (KBD_*), or -1 on error.
 */
static inline int config_get_kbd(void)
{
	return (int)config_get(CONFIG_PARAM_KBD);
}

/**
 * config_set_kbd - Set keyboard layout
 * @layout: Keyboard layout (KBD_*)
 * 
 * Returns 0 on success, -1 on error.
 * Requires CAP_SYSTEM capability.
 */
static inline int config_set_kbd(unsigned int layout)
{
	return config_set(CONFIG_PARAM_KBD, layout);
}

/*=============================================================================
 * Limit Configuration Functions
 *============================================================================*/

/**
 * config_get_max_tasks - Get maximum number of tasks
 */
static inline unsigned int config_get_max_tasks(void)
{
	long val = config_get(CONFIG_PARAM_MAX_TASKS);
	return (val < 0) ? CONFIG_MAX_TASKS : (unsigned int)val;
}

/**
 * config_get_max_files - Get maximum open files per task
 */
static inline unsigned int config_get_max_files(void)
{
	long val = config_get(CONFIG_PARAM_MAX_FILES);
	return (val < 0) ? CONFIG_MAX_FILES : (unsigned int)val;
}

/*=============================================================================
 * Console Configuration Functions
 *============================================================================*/

/**
 * config_get_console_size - Get console dimensions
 * @cols: Output columns
 * @rows: Output rows
 */
static inline void config_get_console_size(unsigned int *cols, unsigned int *rows)
{
	if (cols) {
		long val = config_get(CONFIG_PARAM_CONSOLE_COLS);
		*cols = (val < 0) ? CONFIG_CONSOLE_COLS : (unsigned int)val;
	}
	if (rows) {
		long val = config_get(CONFIG_PARAM_CONSOLE_ROWS);
		*rows = (val < 0) ? CONFIG_CONSOLE_ROWS : (unsigned int)val;
	}
}

/*=============================================================================
 * Device Configuration Functions
 *============================================================================*/

/**
 * config_get_boot_dev - Get boot device
 */
static inline unsigned int config_get_boot_dev(void)
{
	long val = config_get(CONFIG_PARAM_BOOT_DEV);
	return (val < 0) ? CONFIG_BOOT_DEVICE : (unsigned int)val;
}

/**
 * config_get_root_dev - Get root device
 */
static inline unsigned int config_get_root_dev(void)
{
	long val = config_get(CONFIG_PARAM_ROOT_DEV);
	return (val < 0) ? CONFIG_ROOT_DEVICE : (unsigned int)val;
}

/**
 * config_get_swap_dev - Get swap device
 */
static inline unsigned int config_get_swap_dev(void)
{
	long val = config_get(CONFIG_PARAM_SWAP_DEV);
	return (val < 0) ? CONFIG_SWAP_DEVICE : (unsigned int)val;
}

/*=============================================================================
 * Feature Configuration
 *============================================================================*/

/**
 * config_has_feature - Check if feature is enabled
 * @feature: Feature flag (SYS_FEATURE_*)
 * 
 * Returns 1 if feature enabled, 0 otherwise.
 */
static inline int config_has_feature(unsigned int feature)
{
	long val = config_get(CONFIG_PARAM_FEATURES);
	if (val < 0)
		return 0;
	return (val & feature) != 0;
}

/*=============================================================================
 * Compile-time Configuration Checks
 *============================================================================*/

#ifdef CONFIG_MICROKERNEL
#define IS_MICROKERNEL 1
#else
#define IS_MICROKERNEL 0
#endif

#ifdef CONFIG_CAPABILITIES
#define HAS_CAPABILITIES 1
#else
#define HAS_CAPABILITIES 0
#endif

#ifdef CONFIG_IPC
#define HAS_IPC 1
#else
#define HAS_IPC 0
#endif

/*=============================================================================
 * Configuration Assertions
 *============================================================================*/

/* Verify configuration sanity */
#if CONFIG_MAX_TASKS > 256
#error "CONFIG_MAX_TASKS too large"
#endif

#if CONFIG_MAX_PORTS > 1024
#error "CONFIG_MAX_PORTS too large"
#endif

#if CONFIG_IPC_MAX_MSG > 65536
#error "CONFIG_IPC_MAX_MSG too large"
#endif

/*=============================================================================
 * Default Configuration Values
 *============================================================================*/

#ifndef CONFIG_PAGE_SIZE
#define CONFIG_PAGE_SIZE 4096
#endif

#ifndef CONFIG_MAX_TASKS
#define CONFIG_MAX_TASKS 64
#endif

#ifndef CONFIG_MAX_PORTS
#define CONFIG_MAX_PORTS 256
#endif

#ifndef CONFIG_MAX_SERVERS
#define CONFIG_MAX_SERVERS 32
#endif

#ifndef CONFIG_HZ
#define CONFIG_HZ 100
#endif

#endif /* _CONFIG_H */
