/*********************************************************************
 *
 * Copyright (C) 2005,  University of Karlsruhe
 *
 * File path:     afterburn-wedge/l4-common/vm.cc
 * Description:   The L4 state machine for implementing the idle loop,
 *                and the concept of "returning to user".  When returning
 *                to user, enters a dispatch loop, for handling L4 messages.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ********************************************************************/
#include <console.h>
#include INC_WEDGE(sync.h)
#include INC_WEDGE(user.h)

word_t irq_traced = 0;
word_t vector_traced[8];

char assert_string[512] = "ASSERTION:  ";
void debug_hex_to_str( unsigned long val, char *s )
{
    static const char representation[] = "0123456789abcdef";
    bool started = false;
    for( int nibble = 2*sizeof(val) - 1; nibble >= 0; nibble-- )
    {
	unsigned data = (val >> (4*nibble)) & 0xf;
	if( !started && !data )
	    continue;
	started = true;
	*s++ = representation[data] ;
    }
}

void debug_dec_to_str(unsigned long val, char *s)
{
    L4_Word_t divisor;
    int width = 8, digits = 0;

    /* estimate number of spaces and digits */
    for (divisor = 1, digits = 1; val/divisor >= 10; divisor *= 10, digits++);

    /* print spaces */
    for ( ; digits < width; digits++ )
	*s++ = ' ';

    /* print digits */
    do {
	*s++ = (((val/divisor) % 10) + '0');
    } while (divisor /= 10);

}

#if defined(L4KA_ASSERT_SYNC)
char lock_assert_string[] = "LOCK_ASSERT(x, LCKN)";
#endif

void mr_save_t::dump(debug_id_t id)
{
    dprintf(id, "tag %08x eip %08x efl %08x edi %08x esi %08x ebp %08x esp %08x ",
	    tag.raw, get(OFS_MR_SAVE_EIP), get(OFS_MR_SAVE_EFLAGS), get(OFS_MR_SAVE_EDI),
	   get(OFS_MR_SAVE_ESI), get(OFS_MR_SAVE_EBP), get(OFS_MR_SAVE_ESP)); 
    dprintf(id, "ebx %08x edx %08x ecx %08x eax %08x\n", get(OFS_MR_SAVE_EBX),	 
	   get(OFS_MR_SAVE_EDX), get(OFS_MR_SAVE_ECX), get(OFS_MR_SAVE_EAX));
   
}

hiostream_kdebug_t::buffer_t hiostream_kdebug_t::buffer[hiostream_kdebug_t::max_clients];
word_t hiostream_kdebug_t::clients = 0;
cpu_lock_t hiostream_kdebug_t::lock;
bool hiostream_kdebug_t::initialized;
IConsole_handle_t hiostream_kdebug_t::handle;
IConsole_content_t hiostream_kdebug_t::content;
CORBA_Environment hiostream_kdebug_t::env;

#define NR_LINUX_SYSCALLS 285	
static const char *linux_syscall_table[NR_LINUX_SYSCALLS] = 
{
    "restart_syscall         ",
    "exit                    ",
    "fork                    ",
    "read                    ",
    "write                   ",
    "open                    ",
    "close                   ",
    "waitpid                 ",
    "creat                   ",
    "link                    ",
    "unlink                  ",
    "execve                  ",
    "chdir                   ",
    "time                    ",
    "mknod                   ",
    "chmod                   ",
    "lchown                  ",
    "break                   ",
    "oldstat                 ",
    "lseek                   ",
    "getpid                  ",
    "mount                   ",
    "umount                  ",
    "setuid                  ",
    "getuid                  ",
    "stime                   ",
    "ptrace                  ",
    "alarm                   ",
    "oldfstat                ",
    "pause                   ",
    "utime                   ",
    "stty                    ",
    "gtty                    ",
    "access                  ",
    "nice                    ",
    "ftime                   ",
    "sync                    ",
    "kill                    ",
    "rename                  ",
    "mkdir                   ",
    "rmdir                   ",
    "dup                     ",
    "pipe                    ",
    "times                   ",
    "prof                    ",
    "brk                     ",
    "setgid                  ",
    "getgid                  ",
    "signal                  ",
    "geteuid                 ",
    "getegid                 ",
    "acct                    ",
    "umount2                 ",
    "lock                    ",
    "ioctl                   ",
    "fcntl                   ",
    "mpx                     ",
    "setpgid                 ",
    "ulimit                  ",
    "oldolduname             ",
    "umask                   ",
    "chroot                  ",
    "ustat                   ",
    "dup2                    ",
    "getppid                 ",
    "getpgrp                 ",
    "setsid                  ",
    "sigaction               ",
    "sgetmask                ",
    "ssetmask                ",
    "setreuid                ",
    "setregid                ",
    "sigsuspend              ",
    "sigpending              ",
    "sethostname             ",
    "setrlimit               ",
    "getrlimit               ",
    "getrusage               ",
    "gettimeofday            ",
    "settimeofday            ",
    "getgroups               ",
    "setgroups               ",
    "select                  ",
    "symlink                 ",
    "oldlstat                ",
    "readlink                ",
    "uselib                  ",
    "swapon                  ",
    "reboot                  ",
    "readdir                 ",
    "mmap                    ",
    "munmap                  ",
    "truncate                ",
    "ftruncate               ",
    "fchmod                  ",
    "fchown                  ",
    "getpriority             ",
    "setpriority             ",
    "profil                  ",
    "statfs                  ",
    "fstatfs                 ",
    "ioperm                  ",
    "socketcall              ",
    "syslog                  ",
    "setitimer               ",
    "getitimer               ",
    "stat                    ",
    "lstat                   ",
    "fstat                   ",
    "olduname                ",
    "iopl                    ",
    "vhangup                 ",
    "idle                    ",
    "vm86old                 ",
    "wait4                   ",
    "swapoff                 ",
    "sysinfo                 ",
    "ipc                     ",
    "fsync                   ",
    "sigreturn               ",
    "clone                   ",
    "setdomainname           ",
    "uname                   ",
    "modify_ldt              ",
    "adjtimex                ",
    "mprotect                ",
    "sigprocmask             ",
    "create_module           ",
    "init_module             ",
    "delete_module           ",
    "get_kernel_syms         ",
    "quotactl                ",
    "getpgid                 ",
    "fchdir                  ",
    "bdflush                 ",
    "sysfs                   ",
    "personality             ",
    "afs_syscall             ",
    "setfsuid                ",
    "setfsgid                ",
    "_llseek                 ",
    "getdents                ",
    "_newselect              ",
    "flock                   ",
    "msync                   ",
    "readv                   ",
    "writev                  ",
    "getsid                  ",
    "fdatasync               ",
    "_sysctl                 ",
    "mlock                   ",
    "munlock                 ",
    "mlockall                ",
    "munlockall              ",
    "sched_setparam          ",
    "sched_getparam          ",
    "sched_setscheduler      ",
    "sched_getscheduler      ",
    "sched_yield             ",
    "sched_get_priority_max  ",
    "sched_get_priority_min  ",
    "sched_rr_get_interval   ",
    "nanosleep               ",
    "mremap                  ",
    "setresuid               ",
    "getresuid               ",
    "vm86                    ",
    "query_module            ",
    "poll                    ",
    "nfsservctl              ",
    "setresgid               ",
    "getresgid               ",
    "prctl                   ",
    "rt_sigreturn            ",
    "rt_sigaction            ",
    "rt_sigprocmask          ",
    "rt_sigpending           ",
    "rt_sigtimedwait         ",
    "rt_sigqueueinfo         ",
    "rt_sigsuspend           ",
    "pread64                 ",
    "pwrite64                ",
    "chown                   ",
    "getcwd                  ",
    "capget                  ",
    "capset                  ",
    "sigaltstack             ",
    "sendfile                ",
    "getpmsg                 ",
    "putpmsg                 ",
    "vfork                   ",
    "ugetrlimit              ",
    "mmap2                   ",
    "truncate64              ",
    "ftruncate64             ",
    "stat64                  ",
    "lstat64                 ",
    "fstat64                 ",
    "lchown32                ",
    "getuid32                ",
    "getgid32                ",
    "geteuid32               ",
    "getegid32               ",
    "setreuid32              ",
    "setregid32              ",
    "getgroups32             ",
    "setgroups32             ",
    "fchown32                ",
    "setresuid32             ",
    "getresuid32             ",
    "setresgid32             ",
    "getresgid32             ",
    "chown32                 ",
    "setuid32                ",
    "setgid32                ",
    "setfsuid32              ",
    "setfsgid32              ",
    "pivot_root              ",
    "mincore                 ",
    "madvise                 ",
    "getdents64              ",
    "fcntl64                 ",
    "unused222               ",
    "unused223               ",
    "gettid                  ",
    "readahead               ",
    "setxattr                ",
    "lsetxattr               ",
    "fsetxattr               ",
    "getxattr                ",
    "lgetxattr               ",
    "fgetxattr               ",
    "listxattr               ",
    "llistxattr              ",
    "flistxattr              ",
    "removexattr             ",
    "lremovexattr            ",
    "fremovexattr            ",
    "tkill                   ",
    "sendfile64              ",
    "futex                   ",
    "sched_setaffinity       ",
    "sched_getaffinity       ",
    "set_thread_area         ",
    "get_thread_area         ",
    "io_setup                ",
    "io_destroy              ",
    "io_getevents            ",
    "io_submit               ",
    "io_cancel               ",
    "fadvise64               ",
    "unused251               ",
    "exit_group              ",
    "lookup_dcookie          ",
    "epoll_create            ",
    "epoll_ctl               ",
    "epoll_wait              ",
    "remap_file_pages        ",
    "set_tid_address         ",
    "timer_create            ",
    "timer_settime           ",
    "timer_gettime           ",
    "timer_getoverrun        ",
    "timer_delete            ",
    "clock_settime           ",
    "clock_gettime           ",
    "clock_getres            ",
    "clock_nanosleep         ",
    "statfs64                ",
    "fstatfs64               ",
    "tgkill                  ",
    "utimes                  ",
    "fadvise64_64            ",
    "vserver                 ",
    "mbind                   ",
    "get_mempolicy           ",
    "set_mempolicy           ",
    "mq_open                 ",
    "mq_unlink               ",
    "mq_timedsend            ",
    "mq_timedreceive         ",
    "mq_notify               ",
    "mq_getsetattr           ",
    "sys_kexec_load          ",
    "waitid                  ",
};
						
						
void dump_linux_syscall(thread_info_t *ti, bool dir)
{						
    char d = dir ? '<' : '>';

    word_t nr = ti->mr_save.get(OFS_MR_SAVE_EAX);
    
    if (nr < NR_LINUX_SYSCALLS)
    {
	word_t *lx_syscall = (word_t *) linux_syscall_table[nr];
	
	dprintf(debug_syscall, "%c lx syscall %C%C%C%C%C%C TID %t", 
		d, 	
		*(lx_syscall+0), *(lx_syscall+1), *(lx_syscall+2),
		*(lx_syscall+3), *(lx_syscall+4), *(lx_syscall+5),
		ti->get_tid());
    }
    ti->mr_save.dump(debug_syscall+1);

}
