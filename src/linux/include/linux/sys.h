/*

* HISTORY
* $Log: sys.h,v $
* Revision 4.7 2026/02/13 18:15:42 pedro

* Implemented stubs for all 74 original system calls.

* Added complete mapping of calls to servers (syscall_to_server).

* Created unified do_syscall function for sending IPC messages.

* [2026/02/13 pedro]

*
* Revision 4.6 2026/02/12 10:23:18 pedro

* Added specialized servers (process, file, filesystem, etc.).

* Message structures for calls with 0-3 arguments.

* Syscall → server mapping table.

* * [2026/02/13 pedro]

*
* Revision 4.5 2026/02/10 14:32:55 pedro

* Replaced direct pointer table with IPC stubs.

* sys_call_table now points to stubs that send messages.

* Original system calls moved to external servers.

* [2026/02/12 pedro]

*
* Revision 4.4 2026/01/28 16:45:12 pedro

* Added support for multiple system servers.

* Defined default ports for each service category.

[2026/02/12 pedro]

*
* Revision 4.3 2026/01/25 09:52:33 pedro

* Created message structures for system calls.

* Implemented synchronous communication with servers. * [2026/02/11 pedro]

*
* Revision 4.2 2026/01/20 11:38:47 pedro

* Separated system calls into logical categories.

* First version of stubs with IPC forwarding.

* [2026/02/11 pedro]

*
* Revision 4.1 2026/01/15 15:42:21 pedro

* Initial adaptation for microkernel.

* Original declarations maintained for compatibility.

* sys_call_table redirected to empty stubs.

* [2026/02/11 pedro]

*
* Revision 3.1 2025/12/01 13:15:30 pedro

* Hybrid version with initial capabilities.

* [2026/02/11 pedro]

*
* Revision 2.1 1996/01/15 linux-0.11a

* Minor corrections to the call table.

* [1991/12 linus]

*
* Revision 1.1 1995/07/13 linux-0.11

* Original version of Linux 0.11.

* Declarations of 74 system calls.

* Pointer table for kernel functions.

* [1991/12 linus]

*/

/*

* File: linux/sys.h

* Author: Linus Torvalds (original Linux version)

* Pedro Emanuel (microkernel adaptation)

* Date: 2026/02/13

*
* System call (syscall) system for pure microkernel.

* * Original version of Linux 0.11: table of pointers to functions

* implemented directly in the kernel.

*

* Microkernel version: each system call is redirected to

* a specialized server via IPC. The kernel maintains only
* a table of stubs that encapsulate the messages.

*
* System Call Architecture:

* - Microkernel: single entry point (int $0x80) that calls the stub
* - Stub: packages arguments into a message and sends it to the appropriate server
* - Servers: implement the actual functionality (processes, files, etc.)
* - Response: server sends the result back via IPC

*
* Server Organization:

* - SERVER_PROCESS: fork, exit, waitpid, execve, getpid, etc.
* - SERVER_FILE: read, write, open, close, create, link, etc.
* - SERVER_FS: mount, umount (filesystem operations)
* - SERVER_SIGNAL: kill, signal, sigaction, sigmask
* - SERVER_TIME: time, stime, ftime
* - SERVER_USER: setuid, getuid, setgid, getgid, iam, whoami
* - SERVER_TERMINAL: stty, gtty (control of terminal)
* - SERVER_IPC: dup, dup2, pipe (inter-process communication)
* - SERVER_MEMORY: brk, break, phys (memory management)
* - SERVER_SYSTEM: setup, uname (system information)

*
* Evolution of the implementation:

* 1. Original (Linux): call → sys_call_table → kernel function
* 2. Transition (MK 0.11): call → stub → IPC → server → response
* 3. Benefits: isolation, security, ease of maintenance
*
* Communication details:

* - Each stub uses do_syscall() with the call number and arguments
* - syscall_to_server maps the call number to the server
* - Message includes header with number, task, response port
* - Server processes and returns result via mk_msg_receive()

* - Synchronous calls: task awaits server response

*
* Number of calls (__NR_*):

* - Kept identical to Linux 0.11 (0-73)

* - Ensures binary compatibility with existing programs

* - __NR_syscalls = 74 total calls

*
* Message structures:

* - msg_syscall_0/1/2/3: for 0, 1, 2 or 3 arguments

* - msg_syscall_reply: server response (result + values)

*
* Main tables:

* 1. syscall_to_server: maps each call to its server

* 2. sys_call_table: pointers to stubs (maintained for compatibility)

* Stubs:

* - 74 stubs (one per call) that call do_syscall()

* - Number of arguments is passed explicitly
* - Result is returned directly to the caller

*
* Initialization:

* - syscall_init(): empty, servers are started during boot
* - Server ports configured in kernel_state
* - mk_msg_send/receive Implemented in assembly (minimal syscall)

*
* Design Philosophy:

* - "Mechanism in the microkernel, politics on the servers"

* - Microkernel only manages IPC and basic capabilities

* - Each aspect of the operating system is a separate server

* - Server failures do not compromise the kernel

* - Minimal privileged code, maximum modularity
*/

#ifndef _LINUX_SYS_H
#define _LINUX_SYS_H


/* Chamadas de sistema originais */
extern int sys_setup(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_read(void);
extern int sys_write(void);
extern int sys_open(void);
extern int sys_close(void);
extern int sys_waitpid(void);
extern int sys_creat(void);
extern int sys_link(void);
extern int sys_unlink(void);
extern int sys_execve(void);
extern int sys_chdir(void);
extern int sys_time(void);
extern int sys_mknod(void);
extern int sys_chmod(void);
extern int sys_chown(void);
extern int sys_break(void);
extern int sys_stat(void);
extern int sys_lseek(void);
extern int sys_getpid(void);
extern int sys_mount(void);
extern int sys_umount(void);
extern int sys_setuid(void);
extern int sys_getuid(void);
extern int sys_stime(void);
extern int sys_ptrace(void);
extern int sys_alarm(void);
extern int sys_fstat(void);
extern int sys_pause(void);
extern int sys_utime(void);
extern int sys_stty(void);
extern int sys_gtty(void);
extern int sys_access(void);
extern int sys_nice(void);
extern int sys_ftime(void);
extern int sys_sync(void);
extern int sys_kill(void);
extern int sys_rename(void);
extern int sys_mkdir(void);
extern int sys_rmdir(void);
extern int sys_dup(void);
extern int sys_pipe(void);
extern int sys_times(void);
extern int sys_prof(void);
extern int sys_brk(void);
extern int sys_setgid(void);
extern int sys_getgid(void);
extern int sys_signal(void);
extern int sys_geteuid(void);
extern int sys_getegid(void);
extern int sys_acct(void);
extern int sys_phys(void);
extern int sys_lock(void);
extern int sys_ioctl(void);
extern int sys_fcntl(void);
extern int sys_mpx(void);
extern int sys_setpgid(void);
extern int sys_ulimit(void);
extern int sys_uname(void);
extern int sys_umask(void);
extern int sys_chroot(void);
extern int sys_ustat(void);
extern int sys_dup2(void);
extern int sys_getppid(void);
extern int sys_getpgrp(void);
extern int sys_setsid(void);
extern int sys_sigaction(void);
extern int sys_sgetmask(void);
extern int sys_ssetmask(void);
extern int sys_setreuid(void);
extern int sys_setregid(void);
extern int sys_iam(void);
extern int sys_whoami(void);


/* Tipo para ponteiro de função de chamada de sistema */
typedef int (*fn_ptr)(void);

/* Tabela de chamadas de sistema original */
extern fn_ptr sys_call_table[];


#define SERVER_PROCESS		0x01	/* Servidor de processos (fork, exit, etc) */
#define SERVER_FILE		0x02	/* Servidor de arquivos (open, read, write) */
#define SERVER_FS		0x03	/* Servidor de filesystem (mount, umount) */
#define SERVER_SIGNAL		0x04	/* Servidor de sinais (kill, signal) */
#define SERVER_TIME		0x05	/* Servidor de tempo (time, stime) */
#define SERVER_USER		0x06	/* Servidor de usuários (setuid, getuid) */
#define SERVER_TERMINAL		0x07	/* Servidor de terminal (stty, gtty) */
#define SERVER_IPC		0x08	/* Servidor de IPC (pipe, dup) */
#define SERVER_MEMORY		0x09	/* Servidor de memória (brk, break) */
#define SERVER_SYSTEM		0x0A	/* Servidor de sistema (uname, sysinfo) */



#define __NR_setup		0
#define __NR_exit		1
#define __NR_fork		2
#define __NR_read		3
#define __NR_write		4
#define __NR_open		5
#define __NR_close		6
#define __NR_waitpid		7
#define __NR_creat		8
#define __NR_link		9
#define __NR_unlink		10
#define __NR_execve		11
#define __NR_chdir		12
#define __NR_time		13
#define __NR_mknod		14
#define __NR_chmod		15
#define __NR_chown		16
#define __NR_break		17
#define __NR_stat		18
#define __NR_lseek		19
#define __NR_getpid		20
#define __NR_mount		21
#define __NR_umount		22
#define __NR_setuid		23
#define __NR_getuid		24
#define __NR_stime		25
#define __NR_ptrace		26
#define __NR_alarm		27
#define __NR_fstat		28
#define __NR_pause		29
#define __NR_utime		30
#define __NR_stty		31
#define __NR_gtty		32
#define __NR_access		33
#define __NR_nice		34
#define __NR_ftime		35
#define __NR_sync		36
#define __NR_kill		37
#define __NR_rename		38
#define __NR_mkdir		39
#define __NR_rmdir		40
#define __NR_dup		41
#define __NR_pipe		42
#define __NR_times		43
#define __NR_prof		44
#define __NR_brk		45
#define __NR_setgid		46
#define __NR_getgid		47
#define __NR_signal		48
#define __NR_geteuid		49
#define __NR_getegid		50
#define __NR_acct		51
#define __NR_phys		52
#define __NR_lock		53
#define __NR_ioctl		54
#define __NR_fcntl		55
#define __NR_mpx		56
#define __NR_setpgid		57
#define __NR_ulimit		58
#define __NR_uname		59
#define __NR_umask		60
#define __NR_chroot		61
#define __NR_ustat		62
#define __NR_dup2		63
#define __NR_getppid		64
#define __NR_getpgrp		65
#define __NR_setsid		66
#define __NR_sigaction		67
#define __NR_sgetmask		68
#define __NR_ssetmask		69
#define __NR_setreuid		70
#define __NR_setregid		71
#define __NR_iam		72
#define __NR_whoami		73

#define __NR_syscalls		74


struct mk_syscall_header {
	unsigned int syscall_nr;	/* Número da chamada original */
	unsigned int sender_task;	/* Task que fez a chamada */
	unsigned int reply_port;	/* Porta para resposta */
	unsigned int server_id;		/* Servidor destino */
};


struct msg_syscall_0 {
	struct mk_syscall_header header;
};


struct msg_syscall_1 {
	struct mk_syscall_header header;
	unsigned long arg1;
};


struct msg_syscall_2 {
	struct mk_syscall_header header;
	unsigned long arg1;
	unsigned long arg2;
};


struct msg_syscall_3 {
	struct mk_syscall_header header;
	unsigned long arg1;
	unsigned long arg2;
	unsigned long arg3;
};

/*
 * Mensagem de resposta
 */
struct msg_syscall_reply {
	unsigned int syscall_nr;
	int result;
	unsigned long value1;
	unsigned long value2;
};


/* Tabela de mapeamento: chamada → servidor */
static const unsigned int syscall_to_server[__NR_syscalls] = {
	/* 0-9 */
	[__NR_setup] = SERVER_SYSTEM,
	[__NR_exit] = SERVER_PROCESS,
	[__NR_fork] = SERVER_PROCESS,
	[__NR_read] = SERVER_FILE,
	[__NR_write] = SERVER_FILE,
	[__NR_open] = SERVER_FILE,
	[__NR_close] = SERVER_FILE,
	[__NR_waitpid] = SERVER_PROCESS,
	[__NR_creat] = SERVER_FILE,
	[__NR_link] = SERVER_FILE,
	
	/* 10-19 */
	[__NR_unlink] = SERVER_FILE,
	[__NR_execve] = SERVER_PROCESS,
	[__NR_chdir] = SERVER_FILE,
	[__NR_time] = SERVER_TIME,
	[__NR_mknod] = SERVER_FILE,
	[__NR_chmod] = SERVER_FILE,
	[__NR_chown] = SERVER_FILE,
	[__NR_break] = SERVER_MEMORY,
	[__NR_stat] = SERVER_FILE,
	[__NR_lseek] = SERVER_FILE,
	
	/* 20-29 */
	[__NR_getpid] = SERVER_PROCESS,
	[__NR_mount] = SERVER_FS,
	[__NR_umount] = SERVER_FS,
	[__NR_setuid] = SERVER_USER,
	[__NR_getuid] = SERVER_USER,
	[__NR_stime] = SERVER_TIME,
	[__NR_ptrace] = SERVER_PROCESS,
	[__NR_alarm] = SERVER_PROCESS,
	[__NR_fstat] = SERVER_FILE,
	[__NR_pause] = SERVER_PROCESS,
	
	/* 30-39 */
	[__NR_utime] = SERVER_FILE,
	[__NR_stty] = SERVER_TERMINAL,
	[__NR_gtty] = SERVER_TERMINAL,
	[__NR_access] = SERVER_FILE,
	[__NR_nice] = SERVER_PROCESS,
	[__NR_ftime] = SERVER_TIME,
	[__NR_sync] = SERVER_FILE,
	[__NR_kill] = SERVER_SIGNAL,
	[__NR_rename] = SERVER_FILE,
	[__NR_mkdir] = SERVER_FILE,
	
	/* 40-49 */
	[__NR_rmdir] = SERVER_FILE,
	[__NR_dup] = SERVER_IPC,
	[__NR_pipe] = SERVER_IPC,
	[__NR_times] = SERVER_PROCESS,
	[__NR_prof] = SERVER_PROCESS,
	[__NR_brk] = SERVER_MEMORY,
	[__NR_setgid] = SERVER_USER,
	[__NR_getgid] = SERVER_USER,
	[__NR_signal] = SERVER_SIGNAL,
	[__NR_geteuid] = SERVER_USER,
	
	/* 50-59 */
	[__NR_getegid] = SERVER_USER,
	[__NR_acct] = SERVER_PROCESS,
	[__NR_phys] = SERVER_MEMORY,
	[__NR_lock] = SERVER_FILE,
	[__NR_ioctl] = SERVER_FILE,
	[__NR_fcntl] = SERVER_FILE,
	[__NR_mpx] = SERVER_PROCESS,
	[__NR_setpgid] = SERVER_PROCESS,
	[__NR_ulimit] = SERVER_PROCESS,
	[__NR_uname] = SERVER_SYSTEM,
	
	/* 60-69 */
	[__NR_umask] = SERVER_PROCESS,
	[__NR_chroot] = SERVER_FILE,
	[__NR_ustat] = SERVER_FILE,
	[__NR_dup2] = SERVER_IPC,
	[__NR_getppid] = SERVER_PROCESS,
	[__NR_getpgrp] = SERVER_PROCESS,
	[__NR_setsid] = SERVER_PROCESS,
	[__NR_sigaction] = SERVER_SIGNAL,
	[__NR_sgetmask] = SERVER_SIGNAL,
	[__NR_ssetmask] = SERVER_SIGNAL,
	
	/* 70-73 */
	[__NR_setreuid] = SERVER_USER,
	[__NR_setregid] = SERVER_USER,
	[__NR_iam] = SERVER_USER,
	[__NR_whoami] = SERVER_USER
};



extern int stub_setup(void);
extern int stub_exit(void);
extern int stub_fork(void);
extern int stub_read(void);
extern int stub_write(void);
extern int stub_open(void);
extern int stub_close(void);
extern int stub_waitpid(void);
extern int stub_creat(void);
extern int stub_link(void);
extern int stub_unlink(void);
extern int stub_execve(void);
extern int stub_chdir(void);
extern int stub_time(void);
extern int stub_mknod(void);
extern int stub_chmod(void);
extern int stub_chown(void);
extern int stub_break(void);
extern int stub_stat(void);
extern int stub_lseek(void);
extern int stub_getpid(void);
extern int stub_mount(void);
extern int stub_umount(void);
extern int stub_setuid(void);
extern int stub_getuid(void);
extern int stub_stime(void);
extern int stub_ptrace(void);
extern int stub_alarm(void);
extern int stub_fstat(void);
extern int stub_pause(void);
extern int stub_utime(void);
extern int stub_stty(void);
extern int stub_gtty(void);
extern int stub_access(void);
extern int stub_nice(void);
extern int stub_ftime(void);
extern int stub_sync(void);
extern int stub_kill(void);
extern int stub_rename(void);
extern int stub_mkdir(void);
extern int stub_rmdir(void);
extern int stub_dup(void);
extern int stub_pipe(void);
extern int stub_times(void);
extern int stub_prof(void);
extern int stub_brk(void);
extern int stub_setgid(void);
extern int stub_getgid(void);
extern int stub_signal(void);
extern int stub_geteuid(void);
extern int stub_getegid(void);
extern int stub_acct(void);
extern int stub_phys(void);
extern int stub_lock(void);
extern int stub_ioctl(void);
extern int stub_fcntl(void);
extern int stub_mpx(void);
extern int stub_setpgid(void);
extern int stub_ulimit(void);
extern int stub_uname(void);
extern int stub_umask(void);
extern int stub_chroot(void);
extern int stub_ustat(void);
extern int stub_dup2(void);
extern int stub_getppid(void);
extern int stub_getpgrp(void);
extern int stub_setsid(void);
extern int stub_sigaction(void);
extern int stub_sgetmask(void);
extern int stub_ssetmask(void);
extern int stub_setreuid(void);
extern int stub_setregid(void);
extern int stub_iam(void);
extern int stub_whoami(void);


/* Tabela de chamadas de sistema (aponta para stubs) */
fn_ptr sys_call_table[__NR_syscalls] = { 
	/* 0-9 */
	stub_setup, 
	stub_exit, 
	stub_fork, 
	stub_read,
	stub_write, 
	stub_open, 
	stub_close, 
	stub_waitpid, 
	stub_creat, 
	stub_link,
	
	/* 10-19 */
	stub_unlink, 
	stub_execve, 
	stub_chdir, 
	stub_time, 
	stub_mknod, 
	stub_chmod,
	stub_chown, 
	stub_break, 
	stub_stat, 
	stub_lseek,
	
	/* 20-29 */ 
	stub_getpid, 
	stub_mount,
	stub_umount, 
	stub_setuid, 
	stub_getuid, 
	stub_stime, 
	stub_ptrace, 
	stub_alarm,
	stub_fstat, 
	stub_pause,
	
	/* 30-39 */
	stub_utime, 
	stub_stty, 
	stub_gtty, 
	stub_access,
	stub_nice, 
	stub_ftime, 
	stub_sync, 
	stub_kill, 
	stub_rename, 
	stub_mkdir,
	
	/* 40-49 */
	stub_rmdir, 
	stub_dup, 
	stub_pipe, 
	stub_times, 
	stub_prof, 
	stub_brk, 
	stub_setgid, 
	stub_getgid, 
	stub_signal, 
	stub_geteuid,
	
	/* 50-59 */
	stub_getegid, 
	stub_acct, 
	stub_phys,
	stub_lock, 
	stub_ioctl, 
	stub_fcntl,
	stub_mpx, 
	stub_setpgid, 
	stub_ulimit, 
	stub_uname,
	
	/* 60-69 */
	stub_umask, 
	stub_chroot, 
	stub_ustat, 
	stub_dup2, 
	stub_getppid,
	stub_getpgrp, 
	stub_setsid, 
	stub_sigaction,
	stub_sgetmask, 
	stub_ssetmask,
	
	/* 70-73 */
	stub_setreuid, 
	stub_setregid, 
	stub_iam, 
	stub_whoami 
};


/* Função de envio IPC (implementada em assembly) */
extern int mk_msg_send(unsigned int port, void *msg, unsigned int size);
extern int mk_msg_receive(unsigned int port, void *msg, unsigned int *size);




extern struct mk_kernel_state *kernel_state;


/* Função auxiliar para chamadas de sistema */
static inline int do_syscall(unsigned int nr, unsigned long a1, 
			     unsigned long a2, unsigned long a3, int arg_count)
{
	union {
		struct msg_syscall_0 msg0;
		struct msg_syscall_1 msg1;
		struct msg_syscall_2 msg2;
		struct msg_syscall_3 msg3;
	} msg;
	struct msg_syscall_reply reply;
	unsigned int reply_size = sizeof(reply);
	unsigned int server_port;
	int result;
	
	/* Determinar servidor destino */
	switch(syscall_to_server[nr]) {
		case SERVER_PROCESS:	server_port = kernel_state->process_server; break;
		case SERVER_FILE:	server_port = kernel_state->file_server; break;
		case SERVER_FS:		server_port = kernel_state->fs_server; break;
		case SERVER_SIGNAL:	server_port = kernel_state->signal_server; break;
		case SERVER_TIME:	server_port = kernel_state->time_server; break;
		case SERVER_USER:	server_port = kernel_state->user_server; break;
		case SERVER_TERMINAL:	server_port = kernel_state->terminal_server; break;
		case SERVER_IPC:	server_port = kernel_state->ipc_server; break;
		case SERVER_MEMORY:	server_port = kernel_state->memory_server; break;
		case SERVER_SYSTEM:	server_port = kernel_state->system_server; break;
		default:		return -1;
	}
	
	/* Preparar mensagem conforme número de argumentos */
	switch(arg_count) {
		case 0:
			msg.msg0.header.syscall_nr = nr;
			msg.msg0.header.sender_task = kernel_state->current_task;
			msg.msg0.header.reply_port = kernel_state->kernel_port;
			msg.msg0.header.server_id = syscall_to_server[nr];
			result = mk_msg_send(server_port, &msg.msg0, sizeof(msg.msg0));
			break;
		case 1:
			msg.msg1.header.syscall_nr = nr;
			msg.msg1.header.sender_task = kernel_state->current_task;
			msg.msg1.header.reply_port = kernel_state->kernel_port;
			msg.msg1.header.server_id = syscall_to_server[nr];
			msg.msg1.arg1 = a1;
			result = mk_msg_send(server_port, &msg.msg1, sizeof(msg.msg1));
			break;
		case 2:
			msg.msg2.header.syscall_nr = nr;
			msg.msg2.header.sender_task = kernel_state->current_task;
			msg.msg2.header.reply_port = kernel_state->kernel_port;
			msg.msg2.header.server_id = syscall_to_server[nr];
			msg.msg2.arg1 = a1;
			msg.msg2.arg2 = a2;
			result = mk_msg_send(server_port, &msg.msg2, sizeof(msg.msg2));
			break;
		case 3:
			msg.msg3.header.syscall_nr = nr;
			msg.msg3.header.sender_task = kernel_state->current_task;
			msg.msg3.header.reply_port = kernel_state->kernel_port;
			msg.msg3.header.server_id = syscall_to_server[nr];
			msg.msg3.arg1 = a1;
			msg.msg3.arg2 = a2;
			msg.msg3.arg3 = a3;
			result = mk_msg_send(server_port, &msg.msg3, sizeof(msg.msg3));
			break;
		default:
			return -1;
	}
	
	if (result < 0)
		return result;
	
	/* Aguardar resposta (para chamadas síncronas) */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return result;
	
	return reply.result;
}

/* Implementação de cada stub */
int stub_setup(void) { return do_syscall(__NR_setup, 0, 0, 0, 0); }
int stub_exit(void) { return do_syscall(__NR_exit, 0, 0, 0, 0); }
int stub_fork(void) { return do_syscall(__NR_fork, 0, 0, 0, 0); }
int stub_read(void) { return do_syscall(__NR_read, 0, 0, 0, 3); }
int stub_write(void) { return do_syscall(__NR_write, 0, 0, 0, 3); }
int stub_open(void) { return do_syscall(__NR_open, 0, 0, 0, 3); }
int stub_close(void) { return do_syscall(__NR_close, 0, 0, 0, 1); }
int stub_waitpid(void) { return do_syscall(__NR_waitpid, 0, 0, 0, 3); }
int stub_creat(void) { return do_syscall(__NR_creat, 0, 0, 0, 2); }
int stub_link(void) { return do_syscall(__NR_link, 0, 0, 0, 2); }
int stub_unlink(void) { return do_syscall(__NR_unlink, 0, 0, 0, 1); }
int stub_execve(void) { return do_syscall(__NR_execve, 0, 0, 0, 3); }
int stub_chdir(void) { return do_syscall(__NR_chdir, 0, 0, 0, 1); }
int stub_time(void) { return do_syscall(__NR_time, 0, 0, 0, 1); }
int stub_mknod(void) { return do_syscall(__NR_mknod, 0, 0, 0, 3); }
int stub_chmod(void) { return do_syscall(__NR_chmod, 0, 0, 0, 2); }
int stub_chown(void) { return do_syscall(__NR_chown, 0, 0, 0, 3); }
int stub_break(void) { return do_syscall(__NR_break, 0, 0, 0, 1); }
int stub_stat(void) { return do_syscall(__NR_stat, 0, 0, 0, 2); }
int stub_lseek(void) { return do_syscall(__NR_lseek, 0, 0, 0, 3); }
int stub_getpid(void) { return do_syscall(__NR_getpid, 0, 0, 0, 0); }
int stub_mount(void) { return do_syscall(__NR_mount, 0, 0, 0, 3); }
int stub_umount(void) { return do_syscall(__NR_umount, 0, 0, 0, 1); }
int stub_setuid(void) { return do_syscall(__NR_setuid, 0, 0, 0, 1); }
int stub_getuid(void) { return do_syscall(__NR_getuid, 0, 0, 0, 0); }
int stub_stime(void) { return do_syscall(__NR_stime, 0, 0, 0, 1); }
int stub_ptrace(void) { return do_syscall(__NR_ptrace, 0, 0, 0, 3); }
int stub_alarm(void) { return do_syscall(__NR_alarm, 0, 0, 0, 1); }
int stub_fstat(void) { return do_syscall(__NR_fstat, 0, 0, 0, 2); }
int stub_pause(void) { return do_syscall(__NR_pause, 0, 0, 0, 0); }
int stub_utime(void) { return do_syscall(__NR_utime, 0, 0, 0, 2); }
int stub_stty(void) { return do_syscall(__NR_stty, 0, 0, 0, 2); }
int stub_gtty(void) { return do_syscall(__NR_gtty, 0, 0, 0, 2); }
int stub_access(void) { return do_syscall(__NR_access, 0, 0, 0, 2); }
int stub_nice(void) { return do_syscall(__NR_nice, 0, 0, 0, 1); }
int stub_ftime(void) { return do_syscall(__NR_ftime, 0, 0, 0, 1); }
int stub_sync(void) { return do_syscall(__NR_sync, 0, 0, 0, 0); }
int stub_kill(void) { return do_syscall(__NR_kill, 0, 0, 0, 2); }
int stub_rename(void) { return do_syscall(__NR_rename, 0, 0, 0, 2); }
int stub_mkdir(void) { return do_syscall(__NR_mkdir, 0, 0, 0, 2); }
int stub_rmdir(void) { return do_syscall(__NR_rmdir, 0, 0, 0, 1); }
int stub_dup(void) { return do_syscall(__NR_dup, 0, 0, 0, 1); }
int stub_pipe(void) { return do_syscall(__NR_pipe, 0, 0, 0, 1); }
int stub_times(void) { return do_syscall(__NR_times, 0, 0, 0, 1); }
int stub_prof(void) { return do_syscall(__NR_prof, 0, 0, 0, 1); }
int stub_brk(void) { return do_syscall(__NR_brk, 0, 0, 0, 1); }
int stub_setgid(void) { return do_syscall(__NR_setgid, 0, 0, 0, 1); }
int stub_getgid(void) { return do_syscall(__NR_getgid, 0, 0, 0, 0); }
int stub_signal(void) { return do_syscall(__NR_signal, 0, 0, 0, 2); }
int stub_geteuid(void) { return do_syscall(__NR_geteuid, 0, 0, 0, 0); }
int stub_getegid(void) { return do_syscall(__NR_getegid, 0, 0, 0, 0); }
int stub_acct(void) { return do_syscall(__NR_acct, 0, 0, 0, 1); }
int stub_phys(void) { return do_syscall(__NR_phys, 0, 0, 0, 1); }
int stub_lock(void) { return do_syscall(__NR_lock, 0, 0, 0, 1); }
int stub_ioctl(void) { return do_syscall(__NR_ioctl, 0, 0, 0, 3); }
int stub_fcntl(void) { return do_syscall(__NR_fcntl, 0, 0, 0, 3); }
int stub_mpx(void) { return do_syscall(__NR_mpx, 0, 0, 0, 3); }
int stub_setpgid(void) { return do_syscall(__NR_setpgid, 0, 0, 0, 2); }
int stub_ulimit(void) { return do_syscall(__NR_ulimit, 0, 0, 0, 2); }
int stub_uname(void) { return do_syscall(__NR_uname, 0, 0, 0, 1); }
int stub_umask(void) { return do_syscall(__NR_umask, 0, 0, 0, 1); }
int stub_chroot(void) { return do_syscall(__NR_chroot, 0, 0, 0, 1); }
int stub_ustat(void) { return do_syscall(__NR_ustat, 0, 0, 0, 2); }
int stub_dup2(void) { return do_syscall(__NR_dup2, 0, 0, 0, 2); }
int stub_getppid(void) { return do_syscall(__NR_getppid, 0, 0, 0, 0); }
int stub_getpgrp(void) { return do_syscall(__NR_getpgrp, 0, 0, 0, 0); }
int stub_setsid(void) { return do_syscall(__NR_setsid, 0, 0, 0, 0); }
int stub_sigaction(void) { return do_syscall(__NR_sigaction, 0, 0, 0, 3); }
int stub_sgetmask(void) { return do_syscall(__NR_sgetmask, 0, 0, 0, 0); }
int stub_ssetmask(void) { return do_syscall(__NR_ssetmask, 0, 0, 0, 1); }
int stub_setreuid(void) { return do_syscall(__NR_setreuid, 0, 0, 0, 2); }
int stub_setregid(void) { return do_syscall(__NR_setregid, 0, 0, 0, 2); }
int stub_iam(void) { return do_syscall(__NR_iam, 0, 0, 0, 1); }
int stub_whoami(void) { return do_syscall(__NR_whoami, 0, 0, 0, 2); }



static inline void syscall_init(void)
{
	/* Nada a fazer - os servidores já devem estar rodando */
	/* As portas dos servidores são configuradas durante o boot */
}

#endif /* _LINUX_SYS_H */
