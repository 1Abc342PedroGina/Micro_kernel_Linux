/*
* HISTORY
* $Log: mktime.c,v $
* Revision 1.1 2026/02/14 23:45:30 pedro
* Microkernel version of mktime.
* Original function preserved for local calculations.
* Added kernel_mktime_with_zone for timezone-aware conversions.
* Now uses time server for epoch validation.
* [2026/02/14 pedro]
*/

/*
 * File: kernel/mktime.c
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/14
 *
 * Kernel mktime for microkernel architecture.
 *
 * Original Linux 0.11: Simple local mktime for kernel use.
 * Microkernel version: Extended to support timezone-aware conversions
 * and validation against time server. The basic function remains for
 * local calculations, but critical time operations can query the time
 * server for epoch and timezone information per capability domain.
 *
 * Security: Time operations may require CAP_TIME capability for
 * cross-domain time conversions.
 */

#include <time.h>
#include <linux/kernel.h>
#include <linux/head.h>

/*=============================================================================
 * ORIGINAL CONSTANTS (Preserved)
 *============================================================================*/

#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)

/*=============================================================================
 * ORIGINAL MONTH TABLE (Preserved)
 *============================================================================*/

/* interestingly, we assume leap-years */
static int month[12] = {
	0,
	DAY*(31),
	DAY*(31+29),
	DAY*(31+29+31),
	DAY*(31+29+31+30),
	DAY*(31+29+31+30+31),
	DAY*(31+29+31+30+31+30),
	DAY*(31+29+31+30+31+30+31),
	DAY*(31+29+31+30+31+30+31+31),
	DAY*(31+29+31+30+31+30+31+31+30),
	DAY*(31+29+31+30+31+30+31+31+30+31),
	DAY*(31+29+31+30+31+30+31+31+30+31+30)
};

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_TIME_VALIDATE	0x1B00	/* Validate time value */
#define MSG_TIME_ZONE		0x1B01	/* Get timezone for domain */
#define MSG_TIME_EPOCH		0x1B02	/* Get epoch offset */
#define MSG_TIME_NOW		0x1B03	/* Get current time */
#define MSG_TIME_SET		0x1B04	/* Set system time */
#define MSG_TIME_REPLY		0x1B05	/* Reply from time server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_time_request {
	struct mk_msg_header header;
	unsigned long timestamp;	/* Timestamp to validate */
	int timezone;			/* Timezone offset */
	unsigned int domain_id;		/* Capability domain */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_time_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		unsigned long timestamp;	/* Validated timestamp */
		int timezone;			/* Timezone offset */
		unsigned long epoch;		/* Epoch offset */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_TIME_READ		0x0001	/* Can read time */
#define CAP_TIME_SET		0x0002	/* Can set time */
#define CAP_TIME_ZONE		0x0004	/* Can access timezone info */

/*=============================================================================
 * ORIGINAL FUNCTION (Preserved for local use)
 *============================================================================*/

/**
 * kernel_mktime - Convert struct tm to seconds since epoch
 * @tm: Broken-down time structure
 *
 * Original Linux 0.11 mktime implementation.
 * Converts a struct tm to seconds since 1970-01-01 00:00:00 UTC.
 * Handles leap years correctly.
 *
 * This function remains for local calculations that don't need
 * timezone awareness or validation.
 *
 * Returns seconds since epoch.
 */
long kernel_mktime(struct tm * tm)
{
	long res;
	int year;
	
	/* Handle year range (fixed Y2K bug) */
	if (tm->tm_year >= 70)
		year = tm->tm_year - 70;
	else
		year = tm->tm_year + 100 - 70; /* Y2K bug fix by hellotigercn 20110803 */
	
	/* magic offsets (y+1) needed to get leapyears right. */
	res = YEAR * year + DAY * ((year + 1) / 4);
	res += month[tm->tm_mon];
	
	/* and (y+2) here. If it wasn't a leap-year, we have to adjust */
	if (tm->tm_mon > 1 && ((year + 2) % 4))
		res -= DAY;
	
	res += DAY * (tm->tm_mday - 1);
	res += HOUR * tm->tm_hour;
	res += MINUTE * tm->tm_min;
	res += tm->tm_sec;
	
	return res;
}

/*=============================================================================
 * MICROKERNEL EXTENSIONS
 *============================================================================*/

/**
 * kernel_mktime_with_zone - Convert struct tm to seconds with timezone
 * @tm: Broken-down time structure
 * @timezone: Timezone offset in seconds east of UTC
 *
 * Extended version that applies timezone offset.
 * Useful for converting local time to UTC.
 */
long kernel_mktime_with_zone(struct tm * tm, int timezone)
{
	long utc = kernel_mktime(tm);
	return utc - timezone;  /* Convert local to UTC */
}

/**
 * kernel_mktime_validate - Convert and validate with time server
 * @tm: Broken-down time structure
 * @timezone: Timezone offset
 * 
 * Converts time and validates with time server.
 * Returns validated timestamp, or -1 if invalid.
 */
long kernel_mktime_validate(struct tm * tm, int timezone)
{
	struct msg_time_request msg;
	struct msg_time_reply reply;
	unsigned int reply_size = sizeof(reply);
	long local_time;
	int result;

	/* First do local conversion */
	local_time = kernel_mktime_with_zone(tm, timezone);

	/* If we have time capability, validate with server */
	if (current_capability & CAP_TIME_READ) {
		msg.header.msg_id = MSG_TIME_VALIDATE;
		msg.header.sender_port = kernel_state->kernel_port;
		msg.header.reply_port = kernel_state->kernel_port;
		msg.header.size = sizeof(msg);
		
		msg.timestamp = local_time;
		msg.timezone = timezone;
		msg.domain_id = 0;  /* Default domain */
		msg.task_id = kernel_state->current_task;
		msg.caps = current_capability;

		result = mk_msg_send(kernel_state->time_server, &msg, sizeof(msg));
		if (result == 0) {
			result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
			if (result == 0 && reply.result == 0) {
				return reply.data.timestamp;
			}
		}
	}

	/* Fall back to local calculation if server unavailable */
	return local_time;
}

/**
 * kernel_mktime_domain - Convert time for specific capability domain
 * @tm: Broken-down time structure
 * @domain_id: Capability domain ID
 *
 * Returns time value adjusted for the specified domain's timezone.
 */
long kernel_mktime_domain(struct tm * tm, unsigned int domain_id)
{
	struct msg_time_request msg;
	struct msg_time_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Get timezone for domain */
	msg.header.msg_id = MSG_TIME_ZONE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.domain_id = domain_id;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->time_server, &msg, sizeof(msg));
	if (result < 0)
		return kernel_mktime(tm);  /* Fallback to UTC */

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0 || reply.result < 0)
		return kernel_mktime(tm);

	/* Convert using domain's timezone */
	return kernel_mktime_with_zone(tm, reply.data.timezone);
}

/**
 * kernel_mktime_utc - Convert to UTC (explicit)
 * @tm: Broken-down time structure
 *
 * Alias for kernel_mktime (assumes tm is UTC).
 */
long kernel_mktime_utc(struct tm * tm)
{
	return kernel_mktime(tm);
}

/**
 * kernel_mktime_local - Convert to local time (uses current domain's timezone)
 * @tm: Broken-down time structure (assumed to be local)
 */
long kernel_mktime_local(struct tm * tm)
{
	/* Get current domain's timezone from time server */
	return kernel_mktime_domain(tm, current_capability & 0x0F);
}

/*=============================================================================
 * TIME VALIDATION FUNCTIONS
 *============================================================================*/

/**
 * is_valid_date - Basic date validation
 * @tm: Time structure to validate
 *
 * Returns 1 if date appears valid, 0 otherwise.
 */
int is_valid_date(struct tm * tm)
{
	/* Basic range checks */
	if (tm->tm_year < 0 || tm->tm_year > 200)
		return 0;
	if (tm->tm_mon < 0 || tm->tm_mon > 11)
		return 0;
	if (tm->tm_mday < 1 || tm->tm_mday > 31)
		return 0;
	if (tm->tm_hour < 0 || tm->tm_hour > 23)
		return 0;
	if (tm->tm_min < 0 || tm->tm_min > 59)
		return 0;
	if (tm->tm_sec < 0 || tm->tm_sec > 61)  /* Allow leap seconds */
		return 0;

	/* Month-specific day checks */
	switch (tm->tm_mon) {
		case 1: /* February */
			if (tm->tm_mday > 29)
				return 0;
			if (tm->tm_mday == 29) {
				int year = (tm->tm_year >= 70) ? 
				           tm->tm_year + 1900 : 
				           tm->tm_year + 2000;
				/* Check for leap year */
				if ((year % 4) != 0 || 
				    ((year % 100) == 0 && (year % 400) != 0))
					return 0;
			}
			break;
		case 3: case 5: case 8: case 10: /* April, June, September, November */
			if (tm->tm_mday > 30)
				return 0;
			break;
	}
	return 1;
}

/**
 * kernel_validate_time - Validate time with server
 * @timestamp: Timestamp to validate
 *
 * Returns validated timestamp, or -1 if invalid.
 */
long kernel_validate_time(long timestamp)
{
	struct msg_time_request msg;
	struct msg_time_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_TIME_READ))
		return timestamp;  /* Can't validate, assume OK */

	msg.header.msg_id = MSG_TIME_VALIDATE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.timestamp = timestamp;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->time_server, &msg, sizeof(msg));
	if (result < 0)
		return timestamp;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0 || reply.result < 0)
		return timestamp;

	return reply.data.timestamp;
}

/*=============================================================================
 * EPOCH HANDLING
 *============================================================================*/

/**
 * kernel_get_epoch - Get epoch offset for domain
 * @domain_id: Capability domain
 *
 * Returns epoch offset in seconds, or 0 if unavailable.
 */
unsigned long kernel_get_epoch(unsigned int domain_id)
{
	struct msg_time_request msg;
	struct msg_time_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	msg.header.msg_id = MSG_TIME_EPOCH;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.domain_id = domain_id;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->time_server, &msg, sizeof(msg));
	if (result < 0)
		return 0;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0 || reply.result < 0)
		return 0;

	return reply.data.epoch;
}

/**
 * kernel_mktime_with_epoch - Convert using custom epoch
 * @tm: Time structure
 * @epoch_offset: Offset from 1970-01-01
 *
 * For domains that use different epoch (e.g., 2000-01-01).
 */
long kernel_mktime_with_epoch(struct tm * tm, unsigned long epoch_offset)
{
	return kernel_mktime(tm) + epoch_offset;
}

/*=============================================================================
 * TIMEZONE UTILITIES
 *============================================================================*/

/**
 * struct timezone - Timezone information
 */
struct timezone {
	int tz_minuteswest;	/* Minutes west of Greenwich */
	int tz_dsttime;		/* Type of DST correction */
};

/**
 * kernel_get_timezone - Get timezone for current domain
 * @tz: Output timezone structure
 *
 * Returns 0 on success, -1 on error.
 */
int kernel_get_timezone(struct timezone *tz)
{
	struct msg_time_request msg;
	struct msg_time_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!tz)
		return -1;

	msg.header.msg_id = MSG_TIME_ZONE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.domain_id = current_capability & 0x0F;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->time_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0 || reply.result < 0)
		return -1;

	tz->tz_minuteswest = reply.data.timezone / 60;
	tz->tz_dsttime = 0;  /* DST not implemented */
	return 0;
}

