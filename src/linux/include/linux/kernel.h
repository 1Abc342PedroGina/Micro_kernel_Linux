/*

* HISTORY
* $Log: kernel.h,v $

* Revision 2.5 2026/02/13 16:42:18 pedro

* Added mk_kernel_state structure for centralized management.

* Implemented inline functions for IPC communication with servers.

* Added specific messages for each service (memory, console, etc.).

* [2026/02/13 pedro]

*
* Revision 2.4 2026/02/10 09:15:32 pedro

* Replaced euid verification with capabilities system.

* Added default ports for system servers.

* Defined message codes for kernel operations.

* * [2026/02/13 pedro]

*
* Revision 2.3 2026/01/28 14:23:51 pedro

* Removed direct hardware dependencies.

* Memory functions now use a memory server via IPC.

* Console functions redirected to the console server.

* [2026/02/12 pedro]

*
* Revision 2.2 2026/01/15 11:07:44 pedro

* Initial adaptation for microkernel.

* Original prototypes maintained for compatibility.

* Delegated implementation for external servers.

* [2026/02/12 pedro]

*
* Revision 2.1 2026/02/11 10:30:15 pedro

* Transitional version: hybrid kernel.

* Functions still run in the kernel, but use capabilities. * [2025/02/12 pedro]

*
* Revision 1.1 1995/07/13 linux-0.11

* Original Linux 0.11 header.

* Contains prototypes of basic kernel functions.

* [11991/12 linus]

*

/*

* File: linux/kernel.h

* Author: Linus Torvalds (original Linux version)

* Pedro Emanuel (microkernel adaptation)

* Date: 2026/02/13

*
* Functions and prototypes frequently used by the kernel.

* Original Linux 0.11 version: functions executed directly.

* Microkernel version: functions transformed into IPC messages for
* specialized servers (memory, console, log, terminal).

*
* Design Philosophy:

* - Each kernel service is an independent server
* - Communication via IPC with well-defined ports
* - Security checks based on capabilities
* - Minimal privileged code (only the microkernel)

* * Function Evolution:

* - verify_area: originally checked the user's area, now
* sends a message to the memory server with the task's capabilities
* - panic: originally stopped the system, now notifies the system server
* - printf/printk: originally wrote directly to the console,
* now send to console and log servers via IPC
* - tty_write: originally direct access to the terminal, now sends a message
* to the terminal server
* - malloc/free: originally internal memory management,
* now requests to the memory server with capabilities
* - suser: originally euid verification, now verification
* of capabilities (CAP_ROOT)
*/


#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H


void verify_area(void * addr, int count);

void panic(const char * str);

int printf(const char * fmt, ...);

int printk(const char * fmt, ...);

int tty_write(unsigned ch, char * buf, int count);

void * malloc(unsigned int size);
void free_s(void * obj, int size);
#define free(x) free_s((x), 0)

#define suser() (current_capability & CAP_ROOT)


typedef unsigned int capability_t;

#define CAP_NULL		0x0000	/* Sem capabilities */
#define CAP_ROOT		0x0001	/* Capability de root */
#define CAP_MEMORY		0x0002	/* Capability de memória */
#define CAP_IO			0x0004	/* Capability de I/O */
#define CAP_PROCESS		0x0008	/* Capability de processos */
#define CAP_DEVICE		0x0010	/* Capability de dispositivos */
#define CAP_SYSTEM		0x0020	/* Capability de sistema */
#define CAP_ALL			0xFFFF	/* Todas as capabilities */

extern capability_t current_capability;

#define MK_BOOTSTRAP_PORT	0x0001	/* Porta de bootstrap */
#define MK_KERNEL_PORT		0x0002	/* Porta do kernel */
#define MK_MEMORY_SERVER	0x0003	/* Porta do servidor de memória */
#define MK_CONSOLE_SERVER	0x0004	/* Porta do servidor de console */
#define MK_LOG_SERVER		0x0005	/* Porta do servidor de log */
#define MK_TTY_SERVER		0x0006	/* Porta do servidor de terminal */
#define MK_PROCESS_SERVER	0x0007	/* Porta do servidor de processos */
#define MK_SYSTEM_SERVER	0x0008	/* Porta do servidor de sistema */

#define MSG_MEM_VERIFY		0x0100	/* Verificar área de memória */
#define MSG_MEM_ALLOC		0x0101	/* Alocar memória */
#define MSG_MEM_FREE		0x0102	/* Liberar memória */

#define MSG_CONSOLE_WRITE	0x0200	/* Escrever no console */
#define MSG_LOG_WRITE		0x0201	/* Escrever no log */
#define MSG_PANIC		0x0202	/* Panic do sistema */

#define MSG_TTY_WRITE		0x0300	/* Escrever no terminal */

struct mk_msg_header {
	unsigned int msg_id;		/* ID da mensagem */
	unsigned int sender_port;	/* Porta do remetente */
	unsigned int reply_port;	/* Porta para resposta (opcional) */
	unsigned int size;		/* Tamanho total da mensagem */
};

struct msg_memory_verify {
	struct mk_msg_header header;
	void * addr;			/* Endereço a verificar */
	int count;			/* Número de bytes */
	capability_t caps;		/* Capabilities da task */
};

struct msg_memory_alloc {
	struct mk_msg_header header;
	unsigned int size;		/* Tamanho solicitado */
	capability_t caps;		/* Capabilities da task */
};

struct msg_memory_free {
	struct mk_msg_header header;
	void * obj;			/* Objeto a liberar */
	int size;			/* Tamanho (se conhecido) */
	capability_t caps;		/* Capabilities da task */
};

struct msg_console_write {
	struct mk_msg_header header;
	const char * fmt;		/* Formato da string */
	unsigned int arg_count;	/* Número de argumentos */
	unsigned long args[6];		/* Argumentos (máx 6) */
};

struct msg_panic {
	struct mk_msg_header header;
	const char * str;		/* String do panic */
};

struct msg_tty_write {
	struct mk_msg_header header;
	unsigned int ch;		/* Canal */
	char * buf;			/* Buffer */
	int count;			/* Tamanho */
};

struct mk_kern_kernel_state {
	/* Portas do sistema */
	unsigned int bootstrap_port;	/* Porta de bootstrap */
	unsigned int kernel_port;	/* Porta do kernel */
	
	/* Servidores registrados */
	unsigned int memory_server;	/* Porta do servidor de memória */
	unsigned int console_server;	/* Porta do servidor de console */
	unsigned int log_server;	/* Porta do servidor de log */
	unsigned int tty_server;	/* Porta do servidor de terminal */
	unsigned int process_server;	/* Porta do servidor de processos */
	unsigned int system_server;	/* Porta do servidor de sistema */
	
	/* Estado do sistema */
	unsigned int panic_called;	/* Flag de panic */
	capability_t kernel_caps;	/* Capabilities do kernel */
};

extern struct mk_kernel_state *kernel_state;

static inline int mk_msg_send(unsigned int port, void *msg, unsigned int size)
{
	/* Chamada de sistema mínima - única entrada no kernel */
	unsigned int result;
	__asm__ __volatile__ (
		"int $0x80"	/* Syscall para microkernel */
		: "=a" (result)
		: "0" (MK_IPC_SEND), "b" (port), "c" (msg), "d" (size)
	);
	return result;
}

static inline void verify_area(void * addr, int count)
{
	struct msg_memory_verify msg;
	
	msg.header.msg_id = MSG_MEM_VERIFY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;	/* Sem resposta */
	msg.header.size = sizeof(msg);
	
	msg.addr = addr;
	msg.count = count;
	msg.caps = current_capability;
	
	mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg));
}

static inline void panic(const char * str)
{
	struct msg_panic msg;
	
	msg.header.msg_id = MSG_PANIC;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.str = str;
	
	mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
	
	/* Se o servidor não responder, loop infinito */
	for(;;);
}

static inline int printf(const char * fmt, ...)
{
	struct msg_console_write msg;
	va_list args;
	int result;
	
	msg.header.msg_id = MSG_CONSOLE_WRITE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.fmt = fmt;
	
	va_start(args, fmt);
	/* Copiar argumentos (simplificado) */
	for(msg.arg_count = 0; msg.arg_count < 6 && args; msg.arg_count++) {
		msg.args[msg.arg_count] = va_arg(args, unsigned long);
	}
	va_end(args);
	
	result = mk_msg_send(kernel_state->console_server, &msg, sizeof(msg));
	
	return result;
}

static inline int printk(const char * fmt, ...)
{
	struct msg_console_write msg;
	va_list args;
	int result;
	
	msg.header.msg_id = MSG_LOG_WRITE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.fmt = fmt;
	
	va_start(args, fmt);
	for(msg.arg_count = 0; msg.arg_count < 6 && args; msg.arg_count++) {
		msg.args[msg.arg_count] = va_arg(args, unsigned long);
	}
	va_end(args);
	
	result = mk_msg_send(kernel_state->log_server, &msg, sizeof(msg));
	
	return result;
}

static inline int tty_write(unsigned ch, char * buf, int count)
{
	struct msg_tty_write msg;
	
	msg.header.msg_id = MSG_TTY_WRITE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.ch = ch;
	msg.buf = buf;
	msg.count = count;
	
	return mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
}

static inline void * malloc(unsigned int size)
{
	struct msg_memory_alloc msg;
	
	msg.header.msg_id = MSG_MEM_ALLOC;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;	/* Aguarda resposta */
	msg.header.size = sizeof(msg);
	
	msg.size = size;
	msg.caps = current_capability;
	
	/* Envia e aguarda resposta com o endereço */
	if(mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) {
		/* Retorna endereço da resposta (simplificado) */
		return (void *)msg;	/* Na prática, extrair da resposta */
	}
	
	return NULL;
}

static inline void free_s(void * obj, int size)
{
	struct msg_memory_free msg;
	
	msg.header.msg_id = MSG_MEM_FREE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.obj = obj;
	msg.size = size;
	msg.caps = current_capability;
	
	mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg));
}


static inline void mk_kernel_init(void)
{
	/* Alocar estado do kernel (na prática, feito pelo boot) */
	kernel_state = (struct mk_kernel_state *)0x1000;	/* Exemplo */
	
	/* Configurar portas do sistema */
	kernel_state->bootstrap_port = MK_BOOTSTRAP_PORT;
	kernel_state->kernel_port = MK_KERNEL_PORT;
	
	/* Registrar servidores (via bootstrap) */
	kernel_state->memory_server = MK_MEMORY_SERVER;
	kernel_state->console_server = MK_CONSOLE_SERVER;
	kernel_state->log_server = MK_LOG_SERVER;
	kernel_state->tty_server = MK_TTY_SERVER;
	kernel_state->process_server = MK_PROCESS_SERVER;
	kernel_state->system_server = MK_SYSTEM_SERVER;
	
	/* Capabilities do kernel */
	kernel_state->kernel_caps = CAP_ALL;
	current_capability = CAP_ALL;	/* Task inicial tem todas */
	
	kernel_state->panic_called = 0;
}


#endif /* _LINUX_KERNEL_H */
