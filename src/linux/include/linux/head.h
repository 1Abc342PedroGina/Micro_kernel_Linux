/*

* HISTORY
* $Log: head.h,v $

* Revision 1.3 2026/02/13 15:42:18 pedro

* Added structures for capacity management (mk_capability).

* Redesigned object tables to support a pure microkernel model.

* Added predefined system ports (MEMORY_SERVER_PORT, etc.).

* [2026/02/13 pedro]

*
* Revision 1.2 2026/02/13:23:45 pedro

* Integration of x86 descriptors with the new capacity system.

* Added abstract types for tasks, threads, and IPC ports.

* Created mk_port, mk_message, and mk_task structures.

* * [2026/01/15 pedro]

*
* Revision 1.1 2026/02/12 14:07:32 pedro

* Initial version of the transition header.

* Adaptation of the original linux/head.h to microkernel concepts.

* Legacy x86 structures (gdt, idt, pg_dir) preserved as abstractions.

* [2025/11/20 pedro]

*
* Revision 0.11 1991/12 linux-0.11

* Original Linux 0.11 header with x86 segment descriptors.

* * [1991/07/13 Linus]

*/

/*

* File: linux/head.h

* Author: Linus Torvalds (original Linux version)

* Pedro Emanuel (microkernel adaptation)

* Date: 2026/02/13

*
* Transition header from Linux 0.11 to microkernel architecture.

* Maintains compatibility with x86 descriptors while implementing
* capability abstractions for tasks, IPC ports, and memory objects.

*
* Evolution:

* - Linux 0.11 (1991): Pure x86 segment descriptors
* - MK 0.11 (2026): Microkernel-style capabilities and IPC

*/

#ifndef _LINUX_HEAD_H
#define _LINUX_HEAD_H


typedef struct desc_struct {
	unsigned long a,b;
} desc_table[256];

extern unsigned long pg_dir[1024];
extern desc_table idt, gdt;


#define GDT_NUL		0	/* Capability nula */
#define GDT_CODE	1	/* Capability para código */
#define GDT_DATA	2	/* Capability para dados */
#define GDT_TMP		3	/* Capability temporária */

#define LDT_NUL		0	/* Capability LDT nula */
#define LDT_CODE	1	/* Capability LDT para código */
#define LDT_DATA	2	/* Capability LDT para dados */

typedef unsigned int		task_t;			/* Capability para task */
typedef unsigned int		thread_t;		/* Capability para thread */
typedef unsigned int		port_t;			/* Porta IPC */
typedef unsigned int		host_t;			/* Capability para host */
typedef unsigned int		processor_t;		/* Capability para processador */
typedef unsigned int		memory_object_t;	/* Capability para objeto de memória */
typedef unsigned int		ipc_space_t;		/* Espaço IPC */

#define TASK_NULL		((task_t) 0)
#define THREAD_NULL		((thread_t) 0)
#define PORT_NULL		((port_t) 0)
#define HOST_NULL		((host_t) 0)
#define PROCESSOR_NULL		((processor_t) 0)
#define MEMORY_OBJECT_NULL	((memory_object_t) 0)

#define MK_MSG_SEND		0x0001		/* Enviar mensagem para porta */
#define MK_MSG_RECEIVE		0x0002		/* Receber mensagem de porta */
#define MK_MSG_REPLY		0x0003		/* Responder mensagem */
#define MK_PORT_ALLOCATE	0x0004		/* Alocar nova porta */
#define MK_PORT_DEALLOCATE	0x0005		/* Desalocar porta */
#define MK_PORT_SET		0x0006		/* Definir conjunto de portas */


#define MK_TASK_CREATE		0x0010		/* Criar nova task */
#define MK_TASK_TERMINATE	0x0011		/* Terminar task */
#define MK_TASK_SUSPEND		0x0012		/* Suspender task */
#define MK_TASK_RESUME		0x0013		/* Retomar task */
#define MK_TASK_GET_INFO	0x0014		/* Obter informação da task */


#define MK_VM_MAP		0x0020		/* Mapear objeto de memória */
#define MK_VM_UNMAP		0x0021		/* Desmapear objeto de memória */
#define MK_VM_ALLOCATE		0x0022		/* Alocar memória */
#define MK_VM_DEALLOCATE	0x0023		/* Desalocar memória */
#define MK_VM_PROTECT		0x0024		/* Proteger região de memória */
#define MK_VM_INHERIT		0x0025		/* Definir herança de memória */


#define MK_PROCESSOR_ASSIGN	0x0030		/* Atribuir thread ao processador */
#define MK_PROCESSOR_GET_INFO	0x0031		/* Obter informação do processador */
#define MK_PROCESSOR_SET_POLICY	0x0032		/* Definir política de escalonamento */

#define MK_EXCEPTION_RAISE	0x0040		/* Levantar exceção */
#define MK_EXCEPTION_REGISTER	0x0041		/* Registrar handler de exceção */

struct mk_port {
	unsigned int port_id;		/* ID único da porta */
	unsigned int owner;		/* Task dona da porta */
	unsigned int queue_head;	/* Cabeça da fila de mensagens */
	unsigned int queue_tail;	/* Cauda da fila de mensagens */
	unsigned int queue_count;	/* Número de mensagens na fila */
	unsigned int max_messages;	/* Máximo de mensagens */
};

struct mk_message {
	unsigned int msg_id;		/* ID da mensagem */
	unsigned int sender;		/* Porta do remetente */
	unsigned int receiver;		/* Porta do destinatário */
	unsigned int type;		/* Tipo da mensagem */
	unsigned int size;		/* Tamanho dos dados */
	unsigned long data[2];		/* Dados (ou ponteiro para dados maiores) */
};


struct mk_capability {
	unsigned int object_id;		/* ID do objeto referenciado */
	unsigned int rights;		/* Direitos de acesso */
	unsigned int task;		/* Task que possui a capability */
};


struct mk_task {
	unsigned int task_id;		/* ID único da task */
	unsigned int state;		/* Estado (running, suspended, etc) */
	unsigned int base_priority;	/* Prioridade base */
	unsigned int port_space;	/* Espaço de portas da task */
	unsigned int memory_space;	/* Objeto de memória principal */
	unsigned int exception_port;	/* Porta para exceções */
};

struct mk_memory_object {
	unsigned int obj_id;		/* ID único do objeto */
	unsigned int size_pages;	/* Tamanho em páginas */
	unsigned int task;		/* Task dona (se privado) */
	unsigned int shared;		/* Flag de compartilhamento */
};

struct mk_port_table {
	unsigned int count;
	struct mk_port ports[256];
};

struct mk_task_table {
	unsigned int count;
	struct mk_task tasks[64];
};

struct mk_kernel_state {
	/* Tabelas de objetos */
	struct mk_port_table	port_table;
	struct mk_task_table	task_table;
	
	/* Objetos pré-alocados */
	unsigned int		next_port_id;
	unsigned int		next_task_id;
	unsigned int		next_object_id;
	
	/* Tabelas originais (agora apenas abstrações) */
	desc_table		*gdt_ptr;
	desc_table		*idt_ptr;
	unsigned long		*pg_dir_ptr;
	
	/* Portas do sistema */
	unsigned int		kernel_port;		/* Porta do kernel */
	unsigned int		bootstrap_port;		/* Porta de boot */
	unsigned int		memory_server_port;	/* Porta do servidor de memória */
	unsigned int		task_server_port;	/* Porta do servidor de tasks */
};

extern struct mk_kernel_state *mk_state;

#define MEMORY_SERVER_PORT	1	/* Porta padrão do servidor de memória */

#define TASK_SERVER_PORT	2	/* Porta padrão do servidor de tasks */

#define SCHEDULER_SERVER_PORT	3	/* Porta padrão do servidor de escalonamento */

#define DEVICE_SERVER_PORT	4	/* Porta padrão do servidor de dispositivos */

#endif /* _LINUX_HEAD_H */
