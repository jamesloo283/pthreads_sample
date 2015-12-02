#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <ucontext.h>
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
 * * kill -s SIGTERM pid: to stop the process
 */

static void sigterm_hdl(int sig, siginfo_t *siginfo, void *ctx);
static void sigs_init(void);
static int termsig = 0;

static void
sigterm_hdl(int sig, siginfo_t *siginfo, void *ctx)
{
	ucontext_t *uctx = ctx;
	printf("Sending PID: %ld, UID: %ld\n",
			(long)siginfo->si_pid,
			(long)siginfo->si_uid);
	termsig = 1;
	/* to do: enable and raise default handler for proper clean up */
}

/*
 * see following man pages for details:
 * * sigaction(2)
 * * sigemptyset(3)
 * * sigprocmask(2)
 * * signal(7)
 * *
 */
static void
sigs_init(void)
{
	//sigset_t newsigset;
	//sigset_t origsigset;
	//sigemptyset(&newsigset);
	//sigaddset(&sigset, SIGTERM);
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = &sigterm_hdl;
	act.sa_flags = SA_SIGINFO;

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
