/*
* HISTORY
* $Log: ipc.c,v $
* Revision 1.1 2026/02/15 09:15:30 pedro
* Microkernel IPC implementation.
* Core message passing system for microkernel.
* Port management, message queues, and capability checking.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/ipc.c
 * Author: Pedro Emanuel (microkernel implementation)
 * Date: 2026/02/15
 *
 * Inter-Process Communication core for microkernel architecture.
 *
 * This is the heart of the microkernel - the only code that runs in
 * privileged mode. It manages IPC ports, message queues, and capability
 * validation for all communication between tasks and servers.
 *
 * Security: All IPC operations validate that the caller has the
 * necessary capabilities to access ports and send/receive messages.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <errno.h>

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

#define MAX_PORTS		256	/* Maximum number of IPC ports */
#define MAX_MSG_QUEUE		64	/* Maximum messages per queue */
#define MAX_MSG_SIZE		4096	/* Maximum message size */
#define MAX_REPLY_QUEUE		16	/* Maximum pending replies */

/* Port flags */
#define PORT_FLAG_FREE		0x00	/* Port is free */
#define PORT_FLAG_USED		0x01	/* Port is allocated */
#define PORT_FLAG_RECEIVE	0x02	/* Task waiting to receive */
#define PORT_FLAG_SEND		0x04	/* Task waiting to send */
#define PORT_FLAG_LIMITED	0x08	/* Limited access port */

/* Message flags */
#define MSG_FLAG_NONE		0x00	/* No flags */
#define MSG_FLAG_REPLY		0x01	/* This is a reply */
#define MSG_FLAG_REQUEST	0x02	/* This is a request */
#define MSG_FLAG_BLOCK		0x04	/* Blocking operation */
#define MSG_FLAG_NONBLOCK	0x08	/* Non-blocking operation */

/*=============================================================================
 * DATA STRUCTURES
 *============================================================================*/

/**
 * ipc_message - Internal message structure
 */
struct ipc_message {
	unsigned int msg_id;		/* Message ID */
	unsigned int sender;		/* Sender port */
	unsigned int receiver;		/* Receiver port */
	unsigned int type;		/* Message type */
	unsigned int size;		/* Data size */
	unsigned long data[2];		/* Data (or pointer to data) */
	unsigned int flags;		/* Message flags */
	struct ipc_message *next;	/* Next in queue */
};

/**
 * ipc_port - IPC port structure
 */
struct ipc_port {
	unsigned int port_id;		/* Port ID */
	unsigned int owner;		/* Owner task ID */
	unsigned int flags;		/* Port flags */
	
	/* Message queue */
	struct ipc_message *queue_head;	/* Head of queue */
	struct ipc_message *queue_tail;	/* Tail of queue */
	unsigned int queue_count;	/* Number of messages */
	unsigned int max_messages;	/* Maximum messages allowed */
	
	/* Waiting tasks */
	struct task_struct *recv_wait;	/* Task waiting to receive */
	struct task_struct *send_wait;	/* Task waiting to send */
	
	/* Capabilities */
	capability_t required_caps;	/* Capabilities needed to use */
	unsigned int domain;		/* Capability domain */
};

/**
 * ipc_reply - Pending reply tracking
 */
struct ipc_reply {
	unsigned int request_id;	/* Request message ID */
	unsigned int reply_port;	/* Port to send reply to */
	struct task_struct *waiting_task;	/* Task waiting for reply */
	unsigned long timeout;		/* Reply timeout */
	struct ipc_reply *next;		/* Next in chain */
};

/*=============================================================================
 * GLOBAL STATE
 *============================================================================*/

static struct ipc_port ipc_ports[MAX_PORTS];
static struct ipc_reply *pending_replies = NULL;
static unsigned int next_port_id = PORT_DYNAMIC_START;

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

static int port_validate_access(unsigned int port_id, struct task_struct *task);
static void ipc_queue_message(struct ipc_port *port, struct ipc_message *msg);
static struct ipc_message *ipc_dequeue_message(struct ipc_port *port);
static void ipc_wakeup_sender(struct ipc_port *port);
static void ipc_wakeup_receiver(struct ipc_port *port);
static int ipc_add_reply(unsigned int request_id, unsigned int reply_port,
                          struct task_struct *task, unsigned long timeout);
static struct ipc_reply *ipc_find_reply(unsigned int request_id);

/*=============================================================================
 * PORT MANAGEMENT
 *============================================================================*/

/**
 * ipc_init - Initialize IPC subsystem
 */
void ipc_init(void)
{
	int i;
	
	printk("Initializing IPC subsystem...\n");
	
	/* Initialize all ports as free */
	for (i = 0; i < MAX_PORTS; i++) {
		ipc_ports[i].port_id = i;
		ipc_ports[i].owner = 0;
		ipc_ports[i].flags = PORT_FLAG_FREE;
		ipc_ports[i].queue_head = NULL;
		ipc_ports[i].queue_tail = NULL;
		ipc_ports[i].queue_count = 0;
		ipc_ports[i].max_messages = MAX_MSG_QUEUE;
		ipc_ports[i].recv_wait = NULL;
		ipc_ports[i].send_wait = NULL;
		ipc_ports[i].required_caps = CAP_NULL;
		ipc_ports[i].domain = 0;
	}
	
	/* Initialize reserved ports (0 is invalid, 1-0xFF are system) */
	for (i = PORT_RESERVED_START; i <= PORT_RESERVED_END; i++) {
		ipc_ports[i].flags = PORT_FLAG_USED;
		ipc_ports[i].owner = TASK_ID_KERNEL;
		ipc_ports[i].required_caps = CAP_SYSTEM;
	}
	
	pending_replies = NULL;
	next_port_id = PORT_DYNAMIC_START;
	
	printk("IPC subsystem initialized (%d ports available)\n", 
	       MAX_PORTS - PORT_RESERVED_END);
}

/**
 * ipc_allocate_port - Allocate a new IPC port
 * @owner: Owner task ID
 * @caps: Required capabilities for access
 * 
 * Returns port ID, or -1 on error.
 */
int ipc_allocate_port(unsigned int owner, capability_t caps)
{
	int i;
	
	cli();	/* Disable interrupts during allocation */
	
	for (i = PORT_DYNAMIC_START; i < MAX_PORTS; i++) {
		if (ipc_ports[i].flags == PORT_FLAG_FREE) {
			ipc_ports[i].flags = PORT_FLAG_USED;
			ipc_ports[i].owner = owner;
			ipc_ports[i].required_caps = caps;
			ipc_ports[i].queue_head = NULL;
			ipc_ports[i].queue_tail = NULL;
			ipc_ports[i].queue_count = 0;
			ipc_ports[i].recv_wait = NULL;
			ipc_ports[i].send_wait = NULL;
			
			sti();
			return i;
		}
	}
	
	sti();
	return -1;
}

/**
 * ipc_deallocate_port - Deallocate an IPC port
 * @port_id: Port to deallocate
 * 
 * Returns 0 on success, -1 on error.
 */
int ipc_deallocate_port(unsigned int port_id)
{
	struct ipc_port *port;
	struct ipc_message *msg, *next;
	
	if (port_id >= MAX_PORTS)
		return -EINVAL;
	
	port = &ipc_ports[port_id];
	
	cli();
	
	/* Check if port is allocated */
	if (port->flags == PORT_FLAG_FREE) {
		sti();
		return -EINVAL;
	}
	
	/* Check if caller is owner */
	if (port->owner != kernel_state->current_task) {
		sti();
		return -EPERM;
	}
	
	/* Free all messages in queue */
	msg = port->queue_head;
	while (msg) {
		next = msg->next;
		kfree(msg);
		msg = next;
	}
	
	/* Wake up any waiting tasks */
	if (port->recv_wait)
		wake_up(&port->recv_wait);
	if (port->send_wait)
		wake_up(&port->send_wait);
	
	/* Mark port as free */
	port->flags = PORT_FLAG_FREE;
	port->owner = 0;
	
	sti();
	return 0;
}

/**
 * port_validate_access - Check if task can access port
 * @port_id: Port ID
 * @task: Task attempting access
 * 
 * Returns 1 if access allowed, 0 otherwise.
 */
static int port_validate_access(unsigned int port_id, struct task_struct *task)
{
	struct ipc_port *port;
	
	if (port_id >= MAX_PORTS)
		return 0;
	
	port = &ipc_ports[port_id];
	
	/* Free ports cannot be accessed */
	if (port->flags == PORT_FLAG_FREE)
		return 0;
	
	/* Kernel can access any port */
	if (task->pid == TASK_ID_KERNEL)
		return 1;
	
	/* Check if task has required capabilities */
	if ((task->caps & port->required_caps) != port->required_caps)
		return 0;
	
	/* Check capability domain */
	if (port->domain != 0 && (task->caps & 0x0F) != port->domain)
		return 0;
	
	return 1;
}

/*=============================================================================
 * MESSAGE QUEUE MANAGEMENT
 *============================================================================*/

/**
 * ipc_create_message - Create a new message
 * @msg_id: Message ID
 * @sender: Sender port
 * @receiver: Receiver port
 * @type: Message type
 * @size: Data size
 * @data: Data pointer
 * @flags: Message flags
 * 
 * Returns pointer to new message, or NULL on error.
 */
static struct ipc_message *ipc_create_message(unsigned int msg_id,
                                                unsigned int sender,
                                                unsigned int receiver,
                                                unsigned int type,
                                                unsigned int size,
                                                void *data,
                                                unsigned int flags)
{
	struct ipc_message *msg;
	
	if (size > MAX_MSG_SIZE)
		return NULL;
	
	msg = (struct ipc_message *) kmalloc(sizeof(struct ipc_message));
	if (!msg)
		return NULL;
	
	msg->msg_id = msg_id;
	msg->sender = sender;
	msg->receiver = receiver;
	msg->type = type;
	msg->size = size;
	msg->flags = flags;
	msg->next = NULL;
	
	/* Copy data */
	if (size <= 8) {
		/* Small data fits in message */
		memcpy(msg->data, data, size);
	} else {
		/* Large data - copy to kernel space */
		msg->data[0] = (unsigned long) kmalloc(size);
		if (!msg->data[0]) {
			kfree(msg);
			return NULL;
		}
		memcpy((void *)msg->data[0], data, size);
	}
	
	return msg;
}

/**
 * ipc_free_message - Free a message
 * @msg: Message to free
 */
static void ipc_free_message(struct ipc_message *msg)
{
	if (!msg)
		return;
	
	/* Free large data if allocated */
	if (msg->size > 8 && msg->data[0])
		kfree((void *)msg->data[0]);
	
	kfree(msg);
}

/**
 * ipc_queue_message - Add message to port queue
 * @port: Target port
 * @msg: Message to queue
 */
static void ipc_queue_message(struct ipc_port *port, struct ipc_message *msg)
{
	msg->next = NULL;
	
	if (!port->queue_head) {
		port->queue_head = msg;
		port->queue_tail = msg;
	} else {
		port->queue_tail->next = msg;
		port->queue_tail = msg;
	}
	
	port->queue_count++;
}

/**
 * ipc_dequeue_message - Remove message from port queue
 * @port: Source port
 * 
 * Returns first message in queue, or NULL if empty.
 */
static struct ipc_message *ipc_dequeue_message(struct ipc_port *port)
{
	struct ipc_message *msg = port->queue_head;
	
	if (msg) {
		port->queue_head = msg->next;
		if (!port->queue_head)
			port->queue_tail = NULL;
		port->queue_count--;
	}
	
	return msg;
}

/*=============================================================================
 * WAIT QUEUE MANAGEMENT
 *============================================================================*/

/**
 * ipc_wakeup_sender - Wake up task waiting to send
 * @port: Port with available queue space
 */
static void ipc_wakeup_sender(struct ipc_port *port)
{
	if (port->send_wait) {
		wake_up(&port->send_wait);
		port->send_wait = NULL;
	}
}

/**
 * ipc_wakeup_receiver - Wake up task waiting to receive
 * @port: Port with available message
 */
static void ipc_wakeup_receiver(struct ipc_port *port)
{
	if (port->recv_wait) {
		wake_up(&port->recv_wait);
		port->recv_wait = NULL;
	}
}

/*=============================================================================
 * REPLY TRACKING
 *============================================================================*/

/**
 * ipc_add_reply - Track pending reply
 * @request_id: Request message ID
 * @reply_port: Port to send reply to
 * @task: Task waiting for reply
 * @timeout: Timeout in jiffies
 * 
 * Returns 0 on success, -1 on error.
 */
static int ipc_add_reply(unsigned int request_id, unsigned int reply_port,
                          struct task_struct *task, unsigned long timeout)
{
	struct ipc_reply *reply;
	
	reply = (struct ipc_reply *) kmalloc(sizeof(struct ipc_reply));
	if (!reply)
		return -1;
	
	reply->request_id = request_id;
	reply->reply_port = reply_port;
	reply->waiting_task = task;
	reply->timeout = timeout ? jiffies + timeout : 0;
	reply->next = pending_replies;
	
	pending_replies = reply;
	return 0;
}

/**
 * ipc_find_reply - Find pending reply by request ID
 * @request_id: Request ID to find
 * 
 * Returns reply structure, or NULL if not found.
 */
static struct ipc_reply *ipc_find_reply(unsigned int request_id)
{
	struct ipc_reply *reply, *prev = NULL;
	
	for (reply = pending_replies; reply; prev = reply, reply = reply->next) {
		if (reply->request_id == request_id) {
			/* Remove from list */
			if (prev)
				prev->next = reply->next;
			else
				pending_replies = reply->next;
			return reply;
		}
	}
	
	return NULL;
}

/**
 * ipc_check_reply_timeouts - Check for timed-out replies
 */
static void ipc_check_reply_timeouts(void)
{
	struct ipc_reply *reply, *prev = NULL, *next;
	unsigned long now = jiffies;
	
	for (reply = pending_replies; reply; reply = next) {
		next = reply->next;
		
		if (reply->timeout && reply->timeout < now) {
			/* Timeout - wake up task with error */
			if (reply->waiting_task) {
				reply->waiting_task->signal |= (1 << (SIGALRM-1));
				wake_up(&reply->waiting_task);
			}
			
			/* Remove from list */
			if (prev)
				prev->next = next;
			else
				pending_replies = next;
			
			kfree(reply);
		} else {
			prev = reply;
		}
	}
}

/*=============================================================================
 * CORE IPC OPERATIONS
 *============================================================================*/

/**
 * sys_ipc_send - Send an IPC message
 * @port: Destination port
 * @msg: Message pointer (in user space)
 * @size: Message size
 * @flags: Send flags (blocking/non-blocking)
 * 
 * Returns 0 on success, negative error code on failure.
 */
int sys_ipc_send(unsigned int port, void *msg, unsigned int size, unsigned int flags)
{
	struct ipc_port *dest_port;
	struct ipc_message *kernel_msg;
	struct mk_msg_header *user_header;
	unsigned int msg_size;
	int result = 0;
	
	/* Validate port */
	if (port >= MAX_PORTS)
		return -EINVAL;
	
	dest_port = &ipc_ports[port];
	
	/* Validate access */
	if (!port_validate_access(port, current))
		return -EPERM;
	
	/* Copy message header from user space */
	user_header = (struct mk_msg_header *) kmalloc(sizeof(struct mk_msg_header));
	if (!user_header)
		return -ENOMEM;
	
	memcpy_from_fs(user_header, msg, sizeof(struct mk_msg_header));
	
	msg_size = user_header->size;
	
	if (msg_size > MAX_MSG_SIZE) {
		kfree(user_header);
		return -EINVAL;
	}
	
	/* Create kernel message */
	kernel_msg = ipc_create_message(user_header->msg_id,
	                                 user_header->sender_port,
	                                 port,
	                                 0,
	                                 msg_size,
	                                 msg,
	                                 flags);
	
	kfree(user_header);
	
	if (!kernel_msg)
		return -ENOMEM;
	
	cli();
	
	/* Check if queue is full */
	if (dest_port->queue_count >= dest_port->max_messages) {
		if (flags & MSG_FLAG_NONBLOCK) {
			sti();
			ipc_free_message(kernel_msg);
			return -EAGAIN;
		}
		
		/* Block until space available */
		dest_port->send_wait = current;
		current->state = TASK_INTERRUPTIBLE;
		sti();
		schedule();
		
		/* Try again */
		return sys_ipc_send(port, msg, size, flags);
	}
	
	/* Queue the message */
	ipc_queue_message(dest_port, kernel_msg);
	
	/* Wake up any waiting receiver */
	ipc_wakeup_receiver(dest_port);
	
	sti();
	
	return 0;
}

/**
 * sys_ipc_receive - Receive an IPC message
 * @port: Source port (0 for any)
 * @msg: Buffer for message (user space)
 * @size_ptr: Pointer to message size (input/output)
 * @flags: Receive flags (blocking/non-blocking)
 * 
 * Returns number of bytes received, or negative error code.
 */
int sys_ipc_receive(unsigned int port, void *msg, unsigned int *size_ptr, unsigned int flags)
{
	struct ipc_port *src_port = NULL;
	struct ipc_message *kernel_msg;
	unsigned int max_size;
	int result = 0;
	
	/* Get maximum buffer size from user */
	max_size = get_fs_long((unsigned long *)size_ptr);
	
	if (port) {
		/* Receive from specific port */
		if (port >= MAX_PORTS)
			return -EINVAL;
		
		src_port = &ipc_ports[port];
		
		if (!port_validate_access(port, current))
			return -EPERM;
	}
	
	cli();
	
	if (port) {
		/* Check specific port */
		if (!src_port->queue_head) {
			if (flags & MSG_FLAG_NONBLOCK) {
				sti();
				return -EAGAIN;
			}
			
			/* Block until message arrives */
			src_port->recv_wait = current;
			current->state = TASK_INTERRUPTIBLE;
			sti();
			schedule();
			
			/* Try again */
			return sys_ipc_receive(port, msg, size_ptr, flags);
		}
		
		kernel_msg = ipc_dequeue_message(src_port);
	} else {
		/* Receive from any port owned by this task */
		int i;
		
		for (i = PORT_DYNAMIC_START; i < MAX_PORTS; i++) {
			if (ipc_ports[i].owner == current->pid &&
			    ipc_ports[i].queue_head) {
				src_port = &ipc_ports[i];
				kernel_msg = ipc_dequeue_message(src_port);
				break;
			}
		}
		
		if (!kernel_msg) {
			if (flags & MSG_FLAG_NONBLOCK) {
				sti();
				return -EAGAIN;
			}
			
			/* Block on all owned ports (simplified) */
			current->state = TASK_INTERRUPTIBLE;
			sti();
			schedule();
			
			/* Try again */
			return sys_ipc_receive(port, msg, size_ptr, flags);
		}
	}
	
	/* Wake up any waiting sender */
	ipc_wakeup_sender(src_port);
	
	sti();
	
	/* Copy message to user space */
	if (kernel_msg->size <= max_size) {
		memcpy_to_fs(msg, kernel_msg, kernel_msg->size);
		put_fs_long(kernel_msg->size, (unsigned long *)size_ptr);
		result = kernel_msg->size;
	} else {
		/* Buffer too small */
		put_fs_long(kernel_msg->size, (unsigned long *)size_ptr);
		result = -ENOSPC;
	}
	
	/* Handle replies */
	if (kernel_msg->flags & MSG_FLAG_REQUEST) {
		ipc_add_reply(kernel_msg->msg_id, kernel_msg->sender,
		              NULL, 0);  /* No timeout */
	}
	
	ipc_free_message(kernel_msg);
	
	return result;
}

/**
 * sys_ipc_reply - Send a reply to a request
 * @request_id: Request ID to reply to
 * @msg: Reply message
 * @size: Message size
 * 
 * Returns 0 on success, negative error code on failure.
 */
int sys_ipc_reply(unsigned int request_id, void *msg, unsigned int size)
{
	struct ipc_reply *reply;
	int result;
	
	cli();
	
	reply = ipc_find_reply(request_id);
	
	if (!reply) {
		sti();
		return -EINVAL;
	}
	
	/* Send reply to the stored reply port */
	result = sys_ipc_send(reply->reply_port, msg, size, MSG_FLAG_REPLY);
	
	/* Wake up waiting task if any */
	if (reply->waiting_task) {
		wake_up(&reply->waiting_task);
	}
	
	kfree(reply);
	sti();
	
	return result;
}

/**
 * sys_ipc_port_allocate - Allocate a new IPC port
 * @caps: Required capabilities for access
 * 
 * Returns port ID, or negative error code.
 */
int sys_ipc_port_allocate(capability_t caps)
{
	return ipc_allocate_port(kernel_state->current_task, caps);
}

/**
 * sys_ipc_port_deallocate - Deallocate an IPC port
 * @port: Port to deallocate
 * 
 * Returns 0 on success, negative error code.
 */
int sys_ipc_port_deallocate(unsigned int port)
{
	return ipc_deallocate_port(port);
}

/**
 * sys_ipc_port_set - Set port attributes
 * @port: Port ID
 * @attr: Attributes to set
 * @value: New value
 * 
 * Returns 0 on success, negative error code.
 */
int sys_ipc_port_set(unsigned int port, unsigned int attr, unsigned long value)
{
	struct ipc_port *p;
	
	if (port >= MAX_PORTS)
		return -EINVAL;
	
	p = &ipc_ports[port];
	
	/* Check if caller is owner */
	if (p->owner != kernel_state->current_task)
		return -EPERM;
	
	cli();
	
	switch (attr) {
		case 1: /* Set max messages */
			p->max_messages = value;
			break;
		case 2: /* Set required capabilities */
			p->required_caps = (capability_t)value;
			break;
		case 3: /* Set domain */
			p->domain = value;
			break;
		default:
			sti();
			return -EINVAL;
	}
	
	sti();
	return 0;
}

/*=============================================================================
 * TIMER CALLBACK
 *============================================================================*/

/**
 * ipc_timer - Called by timer interrupt to check timeouts
 */
void ipc_timer(void)
{
	ipc_check_reply_timeouts();
}

/*=============================================================================
 * INITIALIZATION
 *============================================================================*/

/**
 * ipc_late_init - Late initialization after scheduler is ready
 */
void ipc_late_init(void)
{
	printk("IPC subsystem ready.\n");
}
