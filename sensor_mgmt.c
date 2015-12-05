#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <ucontext.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include "sensor_mgmt.h"
#include "sensor_worker.h"
#include "sensor_cli.h"
#include "sensor_common.h"

/*
 * multiple ways to see threads details:
 * * pstree -p `pidof sensormgmt`
 * * ps -aeT | grep sensormgmt
 * * ps -L -o pid,lwp,pri,psr,nice,start,stat,bsdtime,cmd,comm -C sensormgmt
 * * ps H -C sensormgmt -o 'pid tid cmd comm'
 *
 * * kill -l: lists avilable signals
 * * kill -s SIGTERM|SIGHUP|SIGINT pid: to stop the process
 *
 * a few ways to see open files and ports
 * * sudo lsof -i
 * * sudo netstat -lptu
 * * sudo netstat -tulpn
 */


static int sigs_init(void);
static void sigterm_hdl(int sig, siginfo_t *siginfo, void *ctx);
static void sigusr1_hdl(int sig, siginfo_t *siginfo, void *ctx);
static void sigusr2_hdl(int sig, siginfo_t *siginfo, void *ctx);
static void sigintr_hdl(int sig, siginfo_t *siginfo, void *ctx);
static void* sighdl_multithr(void *arg);

/* to do: get rid of printf in sig handlers */
static int termsig = 0;
static int usr1sig = 0;
static int usr2sig = 0;
static int intsig = 0;	/* ctrl-C interrupt */
static int intrsig = 0;	/* SIGSTOP follow by SIGCONT interrupt */
static int sigwaitinfo_err = 0;
static int sigmisshdl_err = 0;

static void
sigusr1_hdl(int sig, siginfo_t *siginfo, void *ctx)
{
#if 0
	/* future use */
	ucontext_t *uctx;
	if (ctx) {
		uctx = ctx;
	}
#endif
	++usr1sig;
}

static void
sigusr2_hdl(int sig, siginfo_t *siginfo, void *ctx)
{
#if 0
	/* future use */
	ucontext_t *uctx;
	if (ctx) {
		uctx = ctx;
	}
#endif
	++usr2sig;
}

static void
sigintr_hdl(int sig, siginfo_t *siginfo, void *ctx)
{
#if 0
	/* future use */
	ucontext_t *uctx;
	if (ctx) {
		uctx = ctx;
	}
#endif
	++intsig;
}

static void
sigterm_hdl(int sig, siginfo_t *siginfo, void *ctx)
{
#if 0
	/* future use */
	ucontext_t *uctx;
	if (ctx) {
		uctx = ctx;
	}
#endif
	termsig = 1;
}

static void*
sighdl_multithr(void *arg)
{
	sigset_t s;
	siginfo_t sf;
	int sig;
	printf("Signal handler: TID: %d, PID: %d\n", GETTID(), getpid());
	while (1) {
		/* include all sigs and wait on them */
		sigfillset(&s);
		sig = sigwaitinfo(&s, &sf);
		if (-1 == sig) {
			/* due to SIGSTOP follow by SIGCONT */
			if (EINTR == errno) {
				sigintr_hdl(sig, &sf, NULL);
				++intrsig;
				continue;
			} else {
				perror("sigwaitinfo(...)");
				++sigwaitinfo_err;
			}
		}

		switch (sig) {
		case SIGTERM:	/* terminate */
		case SIGHUP:	/* hang up */
		case SIGINT:	/* ctrl-C interrupt */
			/* terminate for all these sigs */
			sigterm_hdl(sig, &sf, NULL);
			break;
		case SIGUSR1:
			sigusr1_hdl(sig, &sf, NULL);
			break;
		case SIGUSR2:
			sigusr2_hdl(sig, &sf, NULL);
			break;
		default:
			printf("Miss-handled signal: %d: \n", sig);
			++sigmisshdl_err;
			break;
		}
	}
}

static int
sigs_init(void)
{
	sigset_t allset;
	/* block all sigs initially */
	sigfillset(&allset);
	int ret = pthread_sigmask(SIG_BLOCK, &allset, NULL);
	if (ret) {
		perror("pthread_sigmask(...)");
		return -1;
	}

	/* create the signal handler thread */
	pthread_t sigh_thr;
	ret = pthread_create(&sigh_thr, NULL, &sighdl_multithr, NULL);
	if (ret) {
		perror("pthread_create(..., sighandler, ...)");
		return -1;
	}
	if ( pthread_setname_np(sigh_thr, "sighdl") ) {
		perror("pthread_setname_np(..., sighandler, ...)");
	}

#if 0
	/*
	 * the following method of configuring signal action is for
	 * single thread application. it cannot be used for this app
	 */
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	/* sets up signal handler */
	act.sa_sigaction = &sigterm_hdl;
	/*
	 * this flag configures the use of more portable sigaction as
	 * opposed to the older and not portable signal/sa_handler
	 */
	act.sa_flags = SA_SIGINFO;
	/* now, add all desired sigs to be handled by this action */
	if (sigaction(SIGTERM, &act, NULL) < 0) {
		perror ("sigaction(..., SIGTERM, ...)");
		exit(1);
	}
	/* ctrl-C */
	if (sigaction(SIGINT, &act, NULL) < 0) {
		perror ("sigaction(..., SIGINT, ...)");
		exit(1);
	}
	if (sigaction(SIGHUP, &act, NULL) < 0) {
		perror ("sigaction(..., SIGHUP, ...)");
		exit(1);
	}
#endif
	return 0;
}

int
main(int argc, char **argv)
{
	if (sigs_init()) {
		printf("Failed configuring signals, terminating\n");
		exit(1);
	}

	if (dbgcli_init()) {
		printf("Failed configuring DBG CLI, terminating\n");
		exit(1);
	}

	if (sens_init()) {
		printf("WARNING: one or more sensor threads failed\n");
	}

	/* main robotic logic */
	while (!termsig) {
		sleep(1);
		/* do some work */
	}

	dbgcli_deinit();
	sens_deinit();

	return 0;
}
