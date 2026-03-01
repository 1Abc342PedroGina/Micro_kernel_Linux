/*
* HISTORY
* $Log: unistd.h,v $
* Revision 1.1 2026/02/14 05:30:45 pedro
* Microkernel version of standard Unix functions.
* All syscall macros now use IPC stubs instead of direct int $0x80.
* _syscall0-3 macros rewritten to use mk_msg_send to system servers.
* Function prototypes preserved for compatibility.
* Added capability checking for all operations.
* [2026/02/14 pedro]
*/

/*
* File: unistd.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Standard symbolic constants and types for microkernel architecture.
*
* Original Linux 0.11: Direct system calls via int $0x80 with registers.
* Microkernel version: All system calls are stubs that send IPC messages
* to the appropriate server (process server, file server, etc.).
*
* The _syscall macros have been rewritten to use the do_syscall mechanism
* from sys.h, which handles IPC routing and capability checking.
*/

#ifndef _UNISTD_H
#define _UNISTD_H

/* ok, this may be a joke, but I'm working on it */
#define _POSIX_VERSION 198808L

#define _POSIX_CHOWN_RESTRICTED	/* only root can do a chown (I think..) */
#define _POSIX_NO_TRUNC		/* no pathname truncation (but see in kernel) */
#define _POSIX_VDISABLE '\0'	/* character to disable things like ^C */
/*#define _POSIX_SAVED_IDS */	/* we'll get to this yet */
/*#define _POSIX_JOB_CONTROL */	/* we aren't there quite yet. Soon hopefully */

#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

#ifndef NULL
#define NULL    ((void *)0)
#endif

/* access */
#define F_OK	0
#define X_OK	1
#define W_OK	2
#define R_OK	4

/* lseek */
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

/* _SC stands for System Configuration. We don't use them much */
#define _SC_ARG_MAX		1
#define _SC_CHILD_MAX		2
#define _SC_CLOCKS_PER_SEC	3
#define _SC_NGROUPS_MAX		4
#define _SC_OPEN_MAX		5
#define _SC_JOB_CONTROL		6
#define _SC_SAVED_IDS		7
#define _SC_VERSION		8

/* more (possibly) configurable things - now pathnames */
#define _PC_LINK_MAX		1
#define _PC_MAX_CANON		2
#define _PC_MAX_INPUT		3
#define _PC_NAME_MAX		4
#define _PC_PATH_MAX		5
#define _PC_PIPE_BUF		6
#define _PC_NO_TRUNC		7
#define _PC_VDISABLE		8
#define _PC_CHOWN_RESTRICTED	9

#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <utime.h>

/* Microkernel-specific includes */
#include <linux/kernel.h>
#include <linux/sys.h>

#ifdef __LIBRARY__

/*
 * System call numbers (preserved exactly from original)
 * These are now used by the do_syscall mechanism in sys.h
 */
#define __NR_setup	0	/* used only by init, to get system going */
#define __NR_exit	1
#define __NR_fork	2
#define __NR_read	3
#define __NR_write	4
#define __NR_open	5
#define __NR_close	6
#define __NR_waitpid	7
#define __NR_creat	8
#define __NR_link	9
#define __NR_unlink	10
#define __NR_execve	11
#define __NR_chdir	12
#define __NR_time	13
#define __NR_mknod	14
#define __NR_chmod	15
#define __NR_chown	16
#define __NR_break	17
#define __NR_stat	18
#define __NR_lseek	19
#define __NR_getpid	20
#define __NR_mount	21
#define __NR_umount	22
#define __NR_setuid	23
#define __NR_getuid	24
#define __NR_stime	25
#define __NR_ptrace	26
#define __NR_alarm	27
#define __NR_fstat	28
#define __NR_pause	29
#define __NR_utime	30
#define __NR_stty	31
#define __NR_gtty	32
#define __NR_access	33
#define __NR_nice	34
#define __NR_ftime	35
#define __NR_sync	36
#define __NR_kill	37
#define __NR_rename	38
#define __NR_mkdir	39
#define __NR_rmdir	40
#define __NR_dup	41
#define __NR_pipe	42
#define __NR_times	43
#define __NR_prof	44
#define __NR_brk	45
#define __NR_setgid	46
#define __NR_getgid	47
#define __NR_signal	48
#define __NR_geteuid	49
#define __NR_getegid	50
#define __NR_acct	51
#define __NR_phys	52
#define __NR_lock	53
#define __NR_ioctl	54
#define __NR_fcntl	55
#define __NR_mpx	56
#define __NR_setpgid	57
#define __NR_ulimit	58
#define __NR_uname	59
#define __NR_umask	60
#define __NR_chroot	61
#define __NR_ustat	62
#define __NR_dup2	63
#define __NR_getppid	64
#define __NR_getpgrp	65
#define __NR_setsid	66
#define __NR_sigaction	67
#define __NR_sgetmask	68
#define __NR_ssetmask	69
#define __NR_setreuid	70
#define __NR_setregid	71
#define __NR_iam	72
#define __NR_whoami	73

/*
 * Microkernel syscall macros - rewritten to use do_syscall from sys.h
 * instead of direct int $0x80. This maintains binary compatibility
 * while redirecting all syscalls to the appropriate servers.
 */

#define _syscall0(type,name) \
type name(void) \
{ \
	return (type) do_syscall(__NR_##name, 0, 0, 0, 0); \
}

#define _syscall1(type,name,atype,a) \
type name(atype a) \
{ \
	return (type) do_syscall(__NR_##name, (unsigned long)a, 0, 0, 1); \
}

#define _syscall2(type,name,atype,a,btype,b) \
type name(atype a, btype b) \
{ \
	return (type) do_syscall(__NR_##name, (unsigned long)a, (unsigned long)b, 0, 2); \
}

#define _syscall3(type,name,atype,a,btype,b,ctype,c) \
type name(atype a, btype b, ctype c) \
{ \
	return (type) do_syscall(__NR_##name, (unsigned long)a, (unsigned long)b, (unsigned long)c, 3); \
}

#endif /* __LIBRARY__ */

extern int errno;

/* File operations */
int access(const char * filename, mode_t mode);
int acct(const char * filename);
int chdir(const char * filename);
int chmod(const char * filename, mode_t mode);
int chown(const char * filename, uid_t owner, gid_t group);
int chroot(const char * filename);
int close(int fildes);
int creat(const char * filename, mode_t mode);
int link(const char * filename1, const char * filename2);
int unlink(const char * filename);
int open(const char * filename, int flag, ...);
int read(int fildes, char * buf, off_t count);
int write(int fildes, const char * buf, off_t count);
int lseek(int fildes, off_t offset, int origin);
int dup(int fildes);
int dup2(int oldfd, int newfd);
int pipe(int * fildes);
int ioctl(int fildes, int cmd, ...);
int fcntl(int fildes, int cmd, ...);
int stat(const char * filename, struct stat * stat_buf);
int fstat(int fildes, struct stat * stat_buf);
int mknod(const char * filename, mode_t mode, dev_t dev);
int mount(const char * specialfile, const char * dir, int rwflag);
int umount(const char * specialfile);
int sync(void);
int rename(const char * oldname, const char * newname);
int mkdir(const char * pathname, mode_t mode);
int rmdir(const char * pathname);

/* Process operations */
static int fork(void);
int execve(const char * filename, char ** argv, char ** envp);
int execv(const char * pathname, char ** argv);
int execvp(const char * file, char ** argv);
int execl(const char * pathname, char * arg0, ...);
int execlp(const char * file, char * arg0, ...);
int execle(const char * pathname, char * arg0, ...);
void _exit(int status);
int getpid(void);
int getppid(void);
int getpgrp(void);
pid_t setsid(void);
int setpgrp(void);
int setpgid(pid_t pid, pid_t pgid);
int nice(int val);
int brk(void * end_data_segment);
void * sbrk(ptrdiff_t increment);
int ulimit(int cmd, long limit);
int ptrace(int request, pid_t pid, int addr, int data);
int mpx(int cmd, ...);
int prof(int flag, void * buf, int size);
int phys(int phys_cmd, void * addr, int len);
int lock(int flag, void * addr, int len);

/* Signal operations */
void (*signal(int sig, void (*fn)(int)))(int);
int kill(pid_t pid, int signal);
int pause(void);
int alarm(int sec);
int sigaction(int sig, struct sigaction * act, struct sigaction * oldact);
int sgetmask(void);
int ssetmask(int mask);

/* User/group operations */
int setuid(uid_t uid);
int getuid(void);
int geteuid(void);
int setgid(gid_t gid);
int getgid(void);
int getegid(void);
int setreuid(uid_t ruid, uid_t euid);
int setregid(gid_t rgid, gid_t egid);
int iam(const char * name);
int whoami(char * name, unsigned int size);

/* Time operations */
time_t time(time_t * tloc);
int stime(time_t * tptr);
int ftime(void * tptr);
time_t times(struct tms * tbuf);
int utime(const char * filename, struct utimbuf * times);
int alarm(int sec);

/* System operations */
int uname(struct utsname * name);
int setup(void);
mode_t umask(mode_t mask);
int ustat(dev_t dev, struct ustat * ubuf);
int sysinfo(struct sysinfo * info);

/* Wait operations */
pid_t wait(int * wait_stat);
pid_t waitpid(pid_t pid, int * wait_stat, int options);

/* Terminal operations */
int stty(int fildes, void * arg);
int gtty(int fildes, void * arg);
int ttyname(int fildes, char * buf);

/* Resource operations */
int ulimit(int cmd, long limit);
int acct(const char * filename);

/* Compatibility stubs - actual implementations in servers */

#ifndef __LIBRARY__
/* These are implemented as weak symbols or stubs that call servers */

int access(const char * filename, mode_t mode)
{
	return do_syscall(__NR_access, (unsigned long)filename, (unsigned long)mode, 0, 2);
}

int acct(const char * filename)
{
	return do_syscall(__NR_acct, (unsigned long)filename, 0, 0, 1);
}

int alarm(int sec)
{
	return do_syscall(__NR_alarm, (unsigned long)sec, 0, 0, 1);
}

int brk(void * end_data_segment)
{
	return do_syscall(__NR_brk, (unsigned long)end_data_segment, 0, 0, 1);
}

void * sbrk(ptrdiff_t increment)
{
	/* sbrk is implemented in terms of brk */
	void * current = (void *)do_syscall(__NR_brk, 0, 0, 0, 0);
	if (increment == 0) return current;
	
	void * new = current + increment;
	if (do_syscall(__NR_brk, (unsigned long)new, 0, 0, 1) < 0)
		return (void *)-1;
	return current;
}

int chdir(const char * filename)
{
	return do_syscall(__NR_chdir, (unsigned long)filename, 0, 0, 1);
}

int chmod(const char * filename, mode_t mode)
{
	return do_syscall(__NR_chmod, (unsigned long)filename, (unsigned long)mode, 0, 2);
}

int chown(const char * filename, uid_t owner, gid_t group)
{
	return do_syscall(__NR_chown, (unsigned long)filename, (unsigned long)owner, (unsigned long)group, 3);
}

int chroot(const char * filename)
{
	return do_syscall(__NR_chroot, (unsigned long)filename, 0, 0, 1);
}

int close(int fildes)
{
	return do_syscall(__NR_close, (unsigned long)fildes, 0, 0, 1);
}

int creat(const char * filename, mode_t mode)
{
	return do_syscall(__NR_creat, (unsigned long)filename, (unsigned long)mode, 0, 2);
}

int dup(int fildes)
{
	return do_syscall(__NR_dup, (unsigned long)fildes, 0, 0, 1);
}

int dup2(int oldfd, int newfd)
{
	return do_syscall(__NR_dup2, (unsigned long)oldfd, (unsigned long)newfd, 0, 2);
}

int execve(const char * filename, char ** argv, char ** envp)
{
	return do_syscall(__NR_execve, (unsigned long)filename, (unsigned long)argv, (unsigned long)envp, 3);
}

void _exit(int status)
{
	do_syscall(__NR_exit, (unsigned long)status, 0, 0, 1);
	/* Should not return */
	for(;;);
}

int fcntl(int fildes, int cmd, ...)
{
	/* Only handle 2-arg version for simplicity */
	return do_syscall(__NR_fcntl, (unsigned long)fildes, (unsigned long)cmd, 0, 2);
}

int fork(void)
{
	return do_syscall(__NR_fork, 0, 0, 0, 0);
}

int getpid(void)
{
	return do_syscall(__NR_getpid, 0, 0, 0, 0);
}

int getuid(void)
{
	return do_syscall(__NR_getuid, 0, 0, 0, 0);
}

int geteuid(void)
{
	return do_syscall(__NR_geteuid, 0, 0, 0, 0);
}

int getgid(void)
{
	return do_syscall(__NR_getgid, 0, 0, 0, 0);
}

int getegid(void)
{
	return do_syscall(__NR_getegid, 0, 0, 0, 0);
}

int getppid(void)
{
	return do_syscall(__NR_getppid, 0, 0, 0, 0);
}

pid_t getpgrp(void)
{
	return do_syscall(__NR_getpgrp, 0, 0, 0, 0);
}

int ioctl(int fildes, int cmd, ...)
{
	return do_syscall(__NR_ioctl, (unsigned long)fildes, (unsigned long)cmd, 0, 2);
}

int kill(pid_t pid, int signal)
{
	return do_syscall(__NR_kill, (unsigned long)pid, (unsigned long)signal, 0, 2);
}

int link(const char * filename1, const char * filename2)
{
	return do_syscall(__NR_link, (unsigned long)filename1, (unsigned long)filename2, 0, 2);
}

int lseek(int fildes, off_t offset, int origin)
{
	return do_syscall(__NR_lseek, (unsigned long)fildes, (unsigned long)offset, (unsigned long)origin, 3);
}

int mknod(const char * filename, mode_t mode, dev_t dev)
{
	return do_syscall(__NR_mknod, (unsigned long)filename, (unsigned long)mode, (unsigned long)dev, 3);
}

int mount(const char * specialfile, const char * dir, int rwflag)
{
	return do_syscall(__NR_mount, (unsigned long)specialfile, (unsigned long)dir, (unsigned long)rwflag, 3);
}

int nice(int val)
{
	return do_syscall(__NR_nice, (unsigned long)val, 0, 0, 1);
}

int open(const char * filename, int flag, ...)
{
	/* For 2-arg version (no mode) */
	return do_syscall(__NR_open, (unsigned long)filename, (unsigned long)flag, 0, 2);
}

int pause(void)
{
	return do_syscall(__NR_pause, 0, 0, 0, 0);
}

int pipe(int * fildes)
{
	return do_syscall(__NR_pipe, (unsigned long)fildes, 0, 0, 1);
}

int read(int fildes, char * buf, off_t count)
{
	return do_syscall(__NR_read, (unsigned long)fildes, (unsigned long)buf, (unsigned long)count, 3);
}

int setpgrp(void)
{
	return do_syscall(__NR_setpgrp, 0, 0, 0, 0);
}

int setpgid(pid_t pid, pid_t pgid)
{
	return do_syscall(__NR_setpgid, (unsigned long)pid, (unsigned long)pgid, 0, 2);
}

pid_t setsid(void)
{
	return do_syscall(__NR_setsid, 0, 0, 0, 0);
}

int setuid(uid_t uid)
{
	return do_syscall(__NR_setuid, (unsigned long)uid, 0, 0, 1);
}

int setgid(gid_t gid)
{
	return do_syscall(__NR_setgid, (unsigned long)gid, 0, 0, 1);
}

int setreuid(uid_t ruid, uid_t euid)
{
	return do_syscall(__NR_setreuid, (unsigned long)ruid, (unsigned long)euid, 0, 2);
}

int setregid(gid_t rgid, gid_t egid)
{
	return do_syscall(__NR_setregid, (unsigned long)rgid, (unsigned long)egid, 0, 2);
}

void (*signal(int sig, void (*fn)(int)))(int)
{
	return (void (*)(int))do_syscall(__NR_signal, (unsigned long)sig, (unsigned long)fn, 0, 2);
}

int stat(const char * filename, struct stat * stat_buf)
{
	return do_syscall(__NR_stat, (unsigned long)filename, (unsigned long)stat_buf, 0, 2);
}

int fstat(int fildes, struct stat * stat_buf)
{
	return do_syscall(__NR_fstat, (unsigned long)fildes, (unsigned long)stat_buf, 0, 2);
}

int stime(time_t * tptr)
{
	return do_syscall(__NR_stime, (unsigned long)tptr, 0, 0, 1);
}

int sync(void)
{
	return do_syscall(__NR_sync, 0, 0, 0, 0);
}

time_t time(time_t * tloc)
{
	return do_syscall(__NR_time, (unsigned long)tloc, 0, 0, 1);
}

time_t times(struct tms * tbuf)
{
	return do_syscall(__NR_times, (unsigned long)tbuf, 0, 0, 1);
}

int ulimit(int cmd, long limit)
{
	return do_syscall(__NR_ulimit, (unsigned long)cmd, (unsigned long)limit, 0, 2);
}

mode_t umask(mode_t mask)
{
	return do_syscall(__NR_umask, (unsigned long)mask, 0, 0, 1);
}

int umount(const char * specialfile)
{
	return do_syscall(__NR_umount, (unsigned long)specialfile, 0, 0, 1);
}

int uname(struct utsname * name)
{
	return do_syscall(__NR_uname, (unsigned long)name, 0, 0, 1);
}

int unlink(const char * filename)
{
	return do_syscall(__NR_unlink, (unsigned long)filename, 0, 0, 1);
}

int ustat(dev_t dev, struct ustat * ubuf)
{
	return do_syscall(__NR_ustat, (unsigned long)dev, (unsigned long)ubuf, 0, 2);
}

int utime(const char * filename, struct utimbuf * times)
{
	return do_syscall(__NR_utime, (unsigned long)filename, (unsigned long)times, 0, 2);
}

pid_t waitpid(pid_t pid, int * wait_stat, int options)
{
	return do_syscall(__NR_waitpid, (unsigned long)pid, (unsigned long)wait_stat, (unsigned long)options, 3);
}

pid_t wait(int * wait_stat)
{
	return do_syscall(__NR_waitpid, (unsigned long)-1, (unsigned long)wait_stat, (unsigned long)0, 3);
}

int write(int fildes, const char * buf, off_t count)
{
	return do_syscall(__NR_write, (unsigned long)fildes, (unsigned long)buf, (unsigned long)count, 3);
}

int iam(const char * name)
{
	return do_syscall(__NR_iam, (unsigned long)name, 0, 0, 1);
}

int whoami(char * name, unsigned int size)
{
	return do_syscall(__NR_whoami, (unsigned long)name, (unsigned long)size, 0, 2);
}

int rename(const char * oldname, const char * newname)
{
	return do_syscall(__NR_rename, (unsigned long)oldname, (unsigned long)newname, 0, 2);
}

int mkdir(const char * pathname, mode_t mode)
{
	return do_syscall(__NR_mkdir, (unsigned long)pathname, (unsigned long)mode, 0, 2);
}

int rmdir(const char * pathname)
{
	return do_syscall(__NR_rmdir, (unsigned long)pathname, 0, 0, 1);
}

int sigaction(int sig, struct sigaction * act, struct sigaction * oldact)
{
	return do_syscall(__NR_sigaction, (unsigned long)sig, (unsigned long)act, (unsigned long)oldact, 3);
}

int sgetmask(void)
{
	return do_syscall(__NR_sgetmask, 0, 0, 0, 0);
}

int ssetmask(int mask)
{
	return do_syscall(__NR_ssetmask, (unsigned long)mask, 0, 0, 1);
}

int setup(void)
{
	return do_syscall(__NR_setup, 0, 0, 0, 0);
}

int acct(const char * filename)
{
	return do_syscall(__NR_acct, (unsigned long)filename, 0, 0, 1);
}

int stty(int fildes, void * arg)
{
	return do_syscall(__NR_stty, (unsigned long)fildes, (unsigned long)arg, 0, 2);
}

int gtty(int fildes, void * arg)
{
	return do_syscall(__NR_gtty, (unsigned long)fildes, (unsigned long)arg, 0, 2);
}

int ftime(void * tptr)
{
	return do_syscall(__NR_ftime, (unsigned long)tptr, 0, 0, 1);
}

int ptrace(int request, pid_t pid, int addr, int data)
{
	return do_syscall(__NR_ptrace, (unsigned long)request, (unsigned long)pid, (unsigned long)addr, 4); /* 4 args */
}

int prof(int flag, void * buf, int size)
{
	return do_syscall(__NR_prof, (unsigned long)flag, (unsigned long)buf, (unsigned long)size, 3);
}

int phys(int phys_cmd, void * addr, int len)
{
	return do_syscall(__NR_phys, (unsigned long)phys_cmd, (unsigned long)addr, (unsigned long)len, 3);
}

int lock(int flag, void * addr, int len)
{
	return do_syscall(__NR_lock, (unsigned long)flag, (unsigned long)addr, (unsigned long)len, 3);
}

int mpx(int cmd, ...)
{
	return do_syscall(__NR_mpx, (unsigned long)cmd, 0, 0, 1);
}

#endif /* !__LIBRARY__ */

/* exec family convenience functions */
int execl(const char * pathname, char * arg0, ...)
{
	/* Simplified - real version would build argv array */
	char *argv[2] = { arg0, NULL };
	return execve(pathname, argv, NULL);
}

int execlp(const char * file, char * arg0, ...)
{
	/* Would search PATH */
	return execl(file, arg0, NULL);
}

int execv(const char * pathname, char ** argv)
{
	return execve(pathname, argv, NULL);
}

int execvp(const char * file, char ** argv)
{
	/* Would search PATH */
	return execve(file, argv, NULL);
}

int execle(const char * pathname, char * arg0, ...)
{
	/* Would handle envp */
	return execve(pathname, &arg0, NULL);
}

#endif /* _UNISTD_H */
