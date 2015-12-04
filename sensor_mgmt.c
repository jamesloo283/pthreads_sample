#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <ucontext.h>
#include <errno.h>
#include "sensor_mgmt.h"
#include "sensor_worker.h"
#include "sensor_cli.h"

/*
 * multiple ways to see threads details:
 * * pstree -p `pidof sensormgmt`
 * * ps -aeT | grep sensormgmt
 * * ps -L -o pid,lwp,pri,psr,nice,start,stat,bsdtime,cmd,comm -C sensormgmt
 *
 * * kill -l: lists avilable signals
 * * kill -s SIGTERM|SIGHUP|SIGINT pid: to stop the process
 */

static void sigs_init(void);
static void sigterm_hdl(int sig, siginfo_t *siginfo, void *ctx);
static void sigusr1_hdl(int sig, siginfo_t *siginfo, void *ctx);
static void sigusr2_hdl(int sig, siginfo_t *siginfo, void *ctx);
static void sigintr_hdl(int sig, siginfo_t *siginfo, void *ctx);
static void* sighdl_multithr(void *arg);

/* to do: get rid of printf in sig handlers */
static int termsig = 0;
static int usr1sig = 0;
static int usr2sig = 0;

static void
sigusr1_hdl(int sig, siginfo_t *siginfo, void *ctx)
{
	ucontext_t *uctx;
	if (ctx) {
		uctx = ctx;
	}
	printf("Sending PID: %ld, UID: %ld\n",
			(long)siginfo->si_pid,
			(long)siginfo->si_uid);
	printf("SIGUSR1 received\n");
}

static void
sigusr2_hdl(int sig, siginfo_t *siginfo, void *ctx)
{
	ucontext_t *uctx;
	if (ctx) {
		uctx = ctx;
	}
	printf("Sending PID: %ld, UID: %ld\n",
			(long)siginfo->si_pid,
			(long)siginfo->si_uid);
	printf("SIGUSR2 received\n");
}

static void
sigintr_hdl(int sig, siginfo_t *siginfo, void *ctx)
{
	ucontext_t *uctx;
	if (ctx) {
		uctx = ctx;
	}
	printf("Sending PID: %ld, UID: %ld\n",
			(long)siginfo->si_pid,
			(long)siginfo->si_uid);
	printf("SIGINTR received\n");
}

static void
sigterm_hdl(int sig, siginfo_t *siginfo, void *ctx)
{
	ucontext_t *uctx;
	if (ctx) {
		uctx = ctx;
	}
	printf("Sending PID: %ld, UID: %ld\n",
			(long)siginfo->si_pid,
			(long)siginfo->si_uid);
	termsig = 1;
}

static void*
sighdl_multithr(void *arg)
{
	sigset_t s;
	siginfo_t sf;
	int sig;
	while (1) {
		/* include all sigs */
		sigfillset(&s);
		sig = sigwaitinfo(&s, &sf);
		if (-1 == sig) {
			perror("sigwaitinfo(...)");
			if (EINTR == errno) {
				sigintr_hdl(sig, &sf, NULL);
			}
			continue;
		}
		switch (sig) {
		case SIGTERM: /* terminate */
		case SIGHUP: /* hang up */
		case SIGINT: /* interrupt from keyboard */
			sigterm_hdl(sig, &sf, NULL);
			break;
		case SIGUSR1:
			sigusr1_hdl(sig, &sf, NULL);
			break;
		case SIGUSR2:
			sigusr2_hdl(sig, &sf, NULL);
			break;
		/* note this (and SIGKILL) can't be caught or ignored */
		case SIGSTOP:
			break;
		case SIGCONT:
			break;
		default:
			printf("Miss-handled signal: %d: \n", sig);
			break;
		}
	}
}


static void
sigs_init(void)
{
	sigset_t allset;
	/* include all sigs */
	sigfillset(&allset);
	/* and block them initially */
	int ret = pthread_sigmask(SIG_BLOCK, &allset, NULL);
	if (ret) {
		perror("pthread_sigmask(...)");
		exit(1);
	}

	/* create the signal handler thread */
	pthread_t sigh_thr;
	ret = pthread_create(&sigh_thr, NULL, &sighdl_multithr, NULL);
	if (ret) {
		perror("pthread_create(..., sighandler, ...)");
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
}

int
main(int argc, char **argv)
{
	sigs_init();
	dbgcli_init();
	sens_init();

	/* main robotic logic */
	while (!termsig) {
		sleep(2);
		printf("in main\n");
	}

	dbgcli_deinit();
	sens_deinit();

	return 0;
}
