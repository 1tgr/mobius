/* $Id: signal.h,v 1.1.1.1 2002/12/31 01:26:20 pavlovskii Exp $ */
/* Copyright (C) 1998 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#ifndef __dj_include_signal_h_
#define __dj_include_signal_h_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __dj_ENFORCE_ANSI_FREESTANDING

#include <sys/types.h>

#define SIGABRT	1
#define SIGFPE	2
#define SIGILL	3
#define SIGSEGV	4
#define SIGTERM	5
#define SIGINT  6

#define SIG_DFL ((void (*)(int))(0))
#define SIG_ERR	((void (*)(int))(1))
#define SIG_IGN	((void (*)(int))(-1))

typedef int sig_atomic_t;

int	raise(int _sig);
void	(*signal(int _sig, void (*_func)(int)))(int);
  
#ifndef __STRICT_ANSI__

#define SA_NOCLDSTOP	1

#define SIGALRM	7
#define SIGHUP	8
/* SIGINT is ansi */
#define SIGKILL	9
#define SIGPIPE	10
#define SIGQUIT	11
#define SIGUSR1	12
#define SIGUSR2	13

#define _SIGMAX 14

#define SIG_BLOCK	1
#define SIG_SETMASK	2
#define SIG_UNBLOCK	3

typedef struct {
  unsigned long __bits[10]; /* max 320 signals */
} sigset_t;

struct sigaction {
  int sa_flags;
  void (*sa_handler)(int);
  sigset_t sa_mask;
};

#ifdef _POSIX_SOURCE
int	kill(pid_t _pid, int _sig);
#endif

int	sigaction(int _sig, const struct sigaction *_act, struct sigaction *_oact);
int	sigaddset(sigset_t *_set, int _signo);
int	sigdelset(sigset_t *_set, int _signo);
int	sigemptyset(sigset_t *_set);
int	sigfillset(sigset_t *_set);
int	sigismember(const sigset_t *_set, int _signo);
int	sigpending(sigset_t *_set);
int	sigprocmask(int _how, const sigset_t *_set, sigset_t *_oset);
int	sigsuspend(const sigset_t *_set);

#endif /* !__STRICT_ANSI__ */
#endif /* !__dj_ENFORCE_ANSI_FREESTANDING */

#ifndef __dj_ENFORCE_FUNCTION_CALLS
#endif /* !__dj_ENFORCE_FUNCTION_CALLS */

#ifdef __cplusplus
}
#endif

#endif /* !__dj_include_signal_h_ */
