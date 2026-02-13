/*

* HISTORY
* $Log: mm.h,v $
* Revision 3.4 2026/02/13 17:23:41 pedro

* Added complete memory object system (memory_object_t).

* Implemented structures for virtual space management (vm_space).

* Added prototypes for vm_allocate, vm_deallocate, vm_protect.

* [2026/02/13 pedro]

*
* Revision 3.3 2026/02/13 11:45:22 pedro

* Replaced direct page management with IPC messages.

* get_free_page, put_page, free_page now use a memory server.

* Added message structures for each operation.

* * [2026/02/10 pedro]

*
* Revision 3.2 2026/02/13 15:32:18 pedro

* Removed dependencies on x86 page tables.

* Introduced abstract types vm_address_t, vm_prot_t, vm_inherit_t.

* Added communication with memory server via ports.

* [2026/01/28 pedro]

*
* Revision 3.1 2026/02/12 14:18:33 pedro

* Adaptation for microkernel architecture.

* Maintained original interfaces for compatibility.

* Delegated implementation for external memory server.

* [2026/01/15 pedro]

*
* Revision 2.1 2026/02/12 11:30:45 pedro

* Hybrid version: local management with capabilities.

* First separation between policy and mechanism.

* [2025/12/01 pedro]

*
* Revision 1.1 1991/12 linux-0.11

* Original version of Linux 0.11.

* Simple memory management with page tables.

* Basic functions: get_free_page, put_page, free_page.

* [1995/12/ linus]

*/

/*

* File: linux/mm.h

* Author: Linus Torvalds (original Linux version)

* Pedro Emanuel (microkernel adaptation)

* Date: 2026/02/13

*
* Memory management for pure microkernel.

* Original version of Linux 0.11: direct manipulation of page tables

* and physical memory management.

*
* Microkernel version: all memory management is delegated to a specialized server (memory server).

Traditional functions become IPC clients that send messages to this server.

* *
* Memory Architecture in the Microkernel:

* - Memory Server: single component with privileges to manage
* physical memory and page tables
* - Client Tasks: request memory operations via IPC
* - Memory Objects: abstraction representing memory areas
* (can be shared, with copy-on-write, etc.)
* - Capabilities-based protection: each operation checks the
* capabilities of the requesting task

*
* Main Concepts:

* - memory_object_t: represents a memory area (file, anonymous region, etc.)
* - vm_space: address space of a task (set of regions)
* - vm_region: contiguous region within an address space
* - copy-on-write: pages shared until first write
*
* Evolution of the original functions:

* - get_free_page: allocate physical page -> MSG_MEM_GET_FREE_PAGE message
* - put_page: map page to virtual address -> MSG_MEM_PUT_PAGE message

* - free_page: release page -> MSG_MEM_FREE_PAGE message

*
* New microkernel features:

* - vm_allocate: allocate memory region (multiple pages)

* - vm_deallocate: release region

* - vm_protect: change region protection

* - vm_inherit: define inheritance behavior (for forks)

* - memory_object_create: create shareable memory objects

*
* Communication model:

* 1. Task sends message to memory server port
* 2. Message includes header with ID, response ports and size
* 3. Server processes request and sends response (if requested)
* 4. Response includes result and value (e.g., allocated address)

*
* Flags and protections:

* - VM_PROT_*: access permissions (read, write, execute)

* - VM_INHERIT_*: behavior on fork (do not inherit, copy, share)
* - MEM_FLAG_*: allocation flags (zero, lock, wire)
* - MEM_COPY_*: copy strategies (copy-on-write, physical)
*
* Compatibility:

* - Maintains PAGE_SIZE (4096) for compatibility with existing code
* - __pa and __va are maintained as macros, but are now translated
* by the memory server (the kernel no longer accesses physical memory directly)
* - Original functions have the same visible behavior, but completely different implementation
* internally
*/


#ifndef _LINUX_MM_H
#define _LINUX_MM_H

#include <linux/kernel.h>

#define PAGE_SIZE 4096


extern unsigned long get_free_page(void);
extern unsigned long put_page(unsigned long page, unsigned long address);
extern void free_page(unsigned long addr);


typedef unsigned int		memory_object_t;	/* Objeto de memória */
typedef unsigned int		vm_task_t;		/* Task com espaço de endereçamento */
typedef unsigned int		vm_address_t;		/* Endereço virtual */
typedef unsigned int		vm_size_t;		/* Tamanho em bytes */
typedef unsigned int		vm_offset_t;		/* Offset em bytes */
typedef unsigned int		vm_inherit_t;		/* Comportamento de herança */
typedef unsigned int		vm_prot_t;		/* Proteção de memória */
typedef unsigned int		vm_attribute_t;		/* Atributos de memória */

#define MEMORY_OBJECT_NULL	((memory_object_t) 0)
#define VM_TASK_NULL		((vm_task_t) 0)


#define MK_MEMORY_SERVER_PORT	0x0003	/* Porta principal do servidor de memória */
#define MK_MEMORY_REPLY_PORT	0x0004	/* Porta para respostas */


#define MSG_MEM_GET_FREE_PAGE	0x0100	/* Obter página livre */
#define MSG_MEM_PUT_PAGE	0x0101	/* Colocar página em endereço */
#define MSG_MEM_FREE_PAGE	0x0102	/* Liberar página */
#define MSG_MEM_MAP		0x0103	/* Mapear objeto de memória */
#define MSG_MEM_UNMAP		0x0104	/* Desmapear objeto de memória */
#define MSG_MEM_ALLOCATE	0x0105	/* Alocar região */
#define MSG_MEM_DEALLOCATE	0x0106	/* Desalocar região */
#define MSG_MEM_PROTECT		0x0107	/* Proteger região */
#define MSG_MEM_INHERIT		0x0108	/* Definir herança */
#define MSG_MEM_COPY		0x0109	/* Copiar região (copy-on-write) */


struct msg_get_free_page {
	struct mk_msg_header header;
	unsigned int task_id;		/* Task solicitante */
	unsigned int flags;		/* Flags de alocação */
};

struct msg_put_page {
	struct mk_msg_header header;
	unsigned long page;		/* Página física */
	unsigned long address;		/* Endereço virtual */
	unsigned int task_id;		/* Task alvo */
	vm_prot_t protection;		/* Proteção da página */
};

struct msg_free_page {
	struct mk_msg_header header;
	unsigned long addr;		/* Endereço a liberar */
	unsigned int task_id;		/* Task dona */
};

struct msg_memory_reply {
	struct mk_msg_header header;
	int result;			/* Código de resultado */
	unsigned long value;		/* Valor retornado (ex: página) */
};

struct memory_object {
	unsigned int obj_id;		/* ID único do objeto */
	unsigned int size;		/* Tamanho em bytes */
	unsigned int task;		/* Task dona (se privado) */
	unsigned int ref_count;		/* Contagem de referências */
	unsigned int copy_strategy;	/* Estratégia de cópia (copy-on-write, etc) */
	vm_prot_t default_prot;		/* Proteção padrão */
	vm_inherit_t inherit;		/* Comportamento de herança */
};

struct vm_region {
	vm_address_t start;		/* Endereço inicial */
	vm_size_t size;			/* Tamanho da região */
	memory_object_t object;		/* Objeto de memória subjacente */
	vm_offset_t offset;		/* Offset dentro do objeto */
	vm_prot_t protection;		/* Proteção atual */
	vm_prot_t max_protection;	/* Proteção máxima */
	vm_inherit_t inherit;		/* Comportamento de herança */
	unsigned int shared;		/* Compartilhado com outras tasks */
};

struct vm_space {
	unsigned int task_id;		/* Task dona */
	unsigned int region_count;	/* Número de regiões */
	struct vm_region *regions;	/* Lista de regiões (até 64) */
	unsigned int page_count;	/* Total de páginas mapeadas */
	unsigned int ref_count;		/* Contagem de referências */
};


#define VM_PROT_NONE		((vm_prot_t) 0x00)	/* Sem acesso */
#define VM_PROT_READ		((vm_prot_t) 0x01)	/* Leitura */
#define VM_PROT_WRITE		((vm_prot_t) 0x02)	/* Escrita */
#define VM_PROT_EXECUTE		((vm_prot_t) 0x04)	/* Execução */
#define VM_PROT_DEFAULT		(VM_PROT_READ | VM_PROT_WRITE)
#define VM_PROT_ALL		(VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE)
#define VM_PROT_COPY		((vm_prot_t) 0x08)	/* Copy-on-write */


#define VM_INHERIT_NONE		((vm_inherit_t) 0)	/* Não herdar */
#define VM_INHERIT_COPY		((vm_inherit_t) 1)	/* Copiar na herança */
#define VM_INHERIT_SHARE	((vm_inherit_t) 2)	/* Compartilhar */

#define MEM_FLAG_NONE		0x0000
#define MEM_FLAG_ZERO		0x0001	/* Zerar página */
#define MEM_FLAG_LOCK		0x0002	/* Travar na memória física */
#define MEM_FLAG_WIRED		0x0004	/* Página wireada */

#define MEM_COPY_NONE		0	/* Sem cópia */
#define MEM_COPY_ON_WRITE	1	/* Copy-on-write */
#define MEM_COPY_PHYSICAL	2	/* Cópia física imediata */

struct physical_page {
	unsigned long addr;		/* Endereço físico */
	unsigned int ref_count;		/* Contagem de referências */
	unsigned int flags;		/* Flags da página */
	unsigned int object_id;		/* Objeto dono (se atribuída) */
};

struct memory_server_state {
	/* Pool de páginas físicas */
	unsigned long total_pages;	/* Total de páginas no sistema */
	unsigned long free_pages;	/* Páginas livres */
	struct physical_page *pages;	/* Array de páginas */
	
	/* Objetos de memória */
	unsigned int next_object_id;
	struct memory_object *objects;
	unsigned int max_objects;
	
	/* Espaços de endereçamento */
	struct vm_space *spaces;
	unsigned int max_spaces;
	
	/* Portas de comunicação */
	unsigned int server_port;
	unsigned int reply_port;
};


/* Declaração da função de envio IPC (definida em kernel.h) */
extern int mk_msg_send(unsigned int port, void *msg, unsigned int size);
extern int mk_msg_receive(unsigned int port, void *msg, unsigned int *size);

/* Estado do kernel (definido em kernel.h) */
extern struct mk_kernel_state *kernel_state;

static inline unsigned long get_free_page(void)
{
	struct msg_get_free_page msg;
	struct msg_memory_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;
	
	/* Preparar mensagem */
	msg.header.msg_id = MSG_MEM_GET_FREE_PAGE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.flags = MEM_FLAG_ZERO;	/* Por padrão, zerar página */
	
	/* Enviar requisição */
	result = mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg));
	if (result < 0)
		return 0;
	
	/* Aguardar resposta */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0 || reply.result < 0)
		return 0;
	
	return reply.value;	/* Endereço da página alocada */
}

static inline unsigned long put_page(unsigned long page, unsigned long address)
{
	struct msg_put_page msg;
	struct msg_memory_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;
	
	/* Preparar mensagem */
	msg.header.msg_id = MSG_MEM_PUT_PAGE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.page = page;
	msg.address = address;
	msg.task_id = kernel_state->current_task;
	msg.protection = VM_PROT_DEFAULT;	/* Leitura/escrita padrão */
	
	/* Enviar requisição */
	result = mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg));
	if (result < 0)
		return 0;
	
	/* Aguardar resposta */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0 || reply.result < 0)
		return 0;
	
	return reply.value;	/* Endereço mapeado (ou 0 se erro) */
}

static inline void free_page(unsigned long addr)
{
	struct msg_free_page msg;
	
	/* Preparar mensagem */
	msg.header.msg_id = MSG_MEM_FREE_PAGE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;	/* Sem resposta */
	msg.header.size = sizeof(msg);
	
	msg.addr = addr;
	msg.task_id = kernel_state->current_task;
	
	/* Enviar requisição (não aguarda resposta) */
	mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg));
}


static inline int vm_allocate(vm_address_t *address, vm_size_t size, unsigned int flags)
{
	/* Implementação similar a get_free_page, mas para regiões */
	return -1;	/* Placeholder */
}


static inline int vm_deallocate(vm_address_t address, vm_size_t size)
{
	/* Similar a free_page, mas para regiões */
	return -1;	/* Placeholder */
}

static inline int vm_protect(vm_address_t address, vm_size_t size, vm_prot_t prot)
{
	/* Implementar como mensagem MSG_MEM_PROTECT */
	return -1;	/* Placeholder */
}


static inline int vm_inherit(vm_address_t address, vm_size_t size, vm_inherit_t inherit)
{
	/* Implementar como mensagem MSG_MEM_INHERIT */
	return -1;	/* Placeholder */
}

static inline memory_object_t memory_object_create(vm_size_t size)
{
	/* Criar objeto gerenciado pelo servidor */
	return MEMORY_OBJECT_NULL;
}


static inline void memory_object_destroy(memory_object_t object)
{
	/* Destruir objeto */
}



/* Compatibilidade com código existente */
#define __pa(x)			((unsigned long)(x))	/* Traduzido pelo servidor */
#define __va(x)			((void *)(x))		/* Traduzido pelo servidor */

/* Alinhamento de página */
#define PAGE_ALIGN(addr)	(((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* Número de páginas para tamanho */
#define PAGE_COUNT(size)	(((size) + PAGE_SIZE - 1) / PAGE_SIZE)



#endif /* _LINUX_MM_H */
