#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include "sensor_cli.h"
#include "sensor_common.h"
#include "sensor_worker.h"

static int cliprocessing = 0;
static pthread_mutex_t clilock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t clitimerlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cliactive = PTHREAD_COND_INITIALIZER;
static pthread_t dbgcli_tid;
static pthread_t clitimer_tid;
static pthread_t cliprocessor_tid;

static void* dbgcli_thr(void *arg);
static void* dbgcli_processor_thr(void *arg);
static void* dbgcli_timer_thr(void *arg);
static int dbgcli_handler(cliargs *args);
static int sen_get(cliargs *arg);
static int sen_set(cliargs *arg);
static int cli_exit(cliargs *arg);

static int
cli_exit(cliargs *arg) {
	return -1;
}

static int
sen_status(cliargs *arg) {
	if (!arg) {
		PRDEBUG("sen_status: invalid args\n");
		return 1;
	}
	if (!arg->usrtoks) {
		PRCLI(arg->clicon, "Invalid arguments\n");
		return 1;
	}
	PRCLIPRETTY(arg->clicon, "sen_status called with args: %s\n", arg->usrtoks);
	return 0;
}

static int
sen_get(cliargs *arg) {
	if (!arg) {
		PRDEBUG("sen_get: invalid args\n");
		return 1;
	}
	if (!arg->usrtoks) {
		PRCLI(arg->clicon, "Invalid arguments\n");
		return 1;
	}
	PRCLIPRETTY(arg->clicon, "sen_get called with args: %s\n", arg->usrtoks);

	sonar_data_t sensdata;
	get_sens_data("sonar", &sensdata);
	PRCLIPRETTY(arg->clicon, "sonar distance_cm: %d, distance_in: %d\n",
			sensdata.distance_cm,
			sensdata.distance_in );
	return 0;
}

static int
sen_set(cliargs *arg) {
	if (!arg) {
		PRDEBUG("sen_set: invalid args\n");
		return 1;
	}
	if (!arg->usrtoks) {
		PRCLI(arg->clicon, "Invalid arguments\n");
		return 0;
	}
	PRCLIPRETTY(arg->clicon, "sen_set called with args: %s\n", arg->usrtoks);
	return 0;
}

clicmds cmdtab[] = {
	{ "set", sen_set },
	{ "get", sen_get },
	{ "status", sen_status },
	{ "exit", cli_exit },
	{ "quit", cli_exit },
	{ "bye", cli_exit },
	{ "q", cli_exit },
};
static int cmdtab_size = sizeof(cmdtab) / sizeof(cmdtab[0]);

static int
dbgcli_handler(cliargs *args)
{
	int len = strlen(args->usrtoks);
	if (0 >= len) {
		/* Empty user input is OK, just return 0 */
		return 0;
	}

	char *worktoks = malloc(len);
	if (!worktoks) {
		PRCLI(args->clicon, "CLI internal error, aborting\n");
		return 1;
	}

	memcpy(worktoks, args->usrtoks, len);
	char delimiters[] = " ";
	char *cmd = strtok(worktoks, delimiters);
	int ucmdlen = strlen(cmd);
	int i, cmdlen;
	for (i = 0; i < cmdtab_size; ++i) {
		cmdlen = strlen(cmdtab[i].cmd);
		if (ucmdlen != cmdlen) {
			continue;
		}
		if (!strncmp(cmdtab[i].cmd, cmd, strlen(cmdtab[i].cmd))) {
			break;
		}
	}
	int ret = 0;
	if (i == cmdtab_size) {
		/* cmd not found, send err msg to CLI prompt */
		PRCLI(args->clicon, "Invalid command: '%s'\n", cmd);
	} else {
		ret = cmdtab[i].func(args);
	}
	if (worktoks) {
		free(worktoks);
	}
	return ret;
}


static void*
dbgcli_thr(void *arg)
{
	/* IPv4 domain, reliable full-duplex stream type, TCP protocol */
	int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (-1 == sfd) {
		PRSYSERR(errno, "Failed creating CLI socket");
		goto done;
	}

	int ret;
	int optval = 1;
	/* socket level option to reuse port/address */
	if ( setsockopt(sfd,
			SOL_SOCKET,
			/*
			 * SO_REUSEPORT doesn't seem to be portable, disable
			 * for now
			 */
			//SO_REUSEPORT | SO_REUSEADDR,
			SO_REUSEADDR,
			&optval,
			sizeof(optval)) ) {
		PRSYSERR(errno, "Failed setting CLI socket option SO_REUSEADDR");
		goto done;
	}

	/* 2 seconds linger to allow data transfer completion upon close */
	struct linger ling;
	memset(&ling, 0, sizeof(ling));
	ling.l_onoff = 1;
	ling.l_linger = 2;
	if ( setsockopt(sfd,
			SOL_SOCKET,
			SO_LINGER,
			(const char *)&ling,
			sizeof(ling)) ) {
		PRSYSERR(errno, "Failed setting CLI socket option SO_LINGER");
		goto done;
	}

	struct sockaddr_in srvaddr;
	memset(&srvaddr, 0, sizeof(srvaddr));
	srvaddr.sin_family = AF_INET;
	/* accept connection on any IP */
	srvaddr.sin_addr.s_addr = INADDR_ANY;
	srvaddr.sin_port = htons(DBGCLIPORT);
	if ( bind(sfd, (struct sockaddr *)&srvaddr, sizeof(srvaddr)) ) {
		PRSYSERR(errno, "Failed binding CLI socket");
		goto done;
	}

	PRDEBUG("CLI thread, TID: %d,  port: %d\n", GETTID(), DBGCLIPORT);

	int clih;
	FILE *clicon;
	cliargs myargs;
	while (1) {
		if ( listen(sfd, SIMULMAXCONN) ) {
			PRSYSERR(errno, "Failed listening on CLI socket");
			goto done;
		}

		socklen_t clilen = sizeof(myargs.cliaddr);
		clih = accept(sfd, (struct sockaddr*)&myargs.cliaddr, &clilen);
		if (-1 == clih) {
			switch (errno) {
			/* for these errs, just continue and retry */
			case EINTR:
			case EAGAIN:
				continue;
				break;
			default:
				PRSYSERR(errno, "Failed accepting CLI connection");
				goto done;
				break;
			}
		}

		pthread_mutex_lock(&clilock);
		if (cliprocessing == 1) {
			clicon = fdopen(clih, "w+");
			if (!clicon) {
				PRSYSERR(errno, "Failed opening CLI connection prompt");
			} else {
				PRCLI(clicon, "A CLI session is already active, aborting\n");
				fclose(clicon);
			}
			close(clih);
			pthread_mutex_unlock(&clilock);
			continue;
		}
		cliprocessing = 1;
		pthread_mutex_unlock(&clilock);

		myargs.clisockh = clih;
		PRDEBUG( "Connected to %s:%u\n",
			inet_ntoa(myargs.cliaddr.sin_addr),
			(unsigned)ntohs(myargs.cliaddr.sin_port) );
		ret = pthread_create(&cliprocessor_tid,
				     NULL,
				     dbgcli_processor_thr,
				     (void*)&myargs);
		if (ret) {
			PRSYSERR(errno, "Failed creating CLI processor thread");
		} else {
			if ( pthread_setname_np(cliprocessor_tid, "cliprocessor") ) {
				PRSYSERR(errno, "Failed setting CLI processor thread name\n");
			}
		}
		ret = pthread_create(&clitimer_tid,
				     NULL,
				     dbgcli_timer_thr,
				     (void*)&myargs);
		if (ret) {
			PRSYSERR(errno, "Failed createing CLI timer thread");
		} else {
			if ( pthread_setname_np(clitimer_tid, "clitimer") ) {
				PRSYSERR(errno, "Failed setting CLI timer thread name");
			}
		}
	}
done:
	pthread_exit(0);
}

static void* dbgcli_timer_thr(void *arg)
{
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	cliargs *uarg = (cliargs*)arg;
	struct timespec ts;
	struct timeval tv;
	int ret;

	pthread_mutex_lock(&clitimerlock);
	while (cliprocessing) {
		(void)gettimeofday(&tv, NULL);
		ts.tv_nsec = tv.tv_usec * 1000;
		ts.tv_sec = tv.tv_sec;
		ts.tv_sec += CLIINACTIVITYSEC;
		ret = pthread_cond_timedwait(&cliactive, &clitimerlock, &ts);
		if (ret == ETIMEDOUT) {
			if (cliprocessing) {
				PRDEBUG("CLI inactivity timedout, cancelling CLI processor thread\n");
				pthread_cancel(cliprocessor_tid);
				PRCLI(uarg->clicon, "\nTerminating CLI due to inactivity\n");
				if (uarg->clicon) {
					fclose(uarg->clicon);
				}
				if (-1 != uarg->clisockh) {
					close(uarg->clisockh);
				}
				if (uarg->usrtoks) {
					free(uarg->usrtoks);
				}
				cliprocessing = 0;
			}
			break;
		}
		PRDEBUG("CLI activity update detected, reseting timer\n");
	}
	pthread_mutex_unlock(&clitimerlock);
	PRDEBUG("CLI processing completed\n");
	pthread_exit(0);
}

static void*
dbgcli_processor_thr(void *args) {
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	cliargs *myargs = (cliargs*)args;
	if (-1 == myargs->clisockh) {
		PRDEBUG("Invalid CLI connection handle\n");
		pthread_exit(0);
	}
	int ret;
	size_t alloclen, readlen;
	char prompt[16];
	snprintf(prompt, sizeof(prompt), "%s", "DBGCLI > ");

	myargs->clicon = fdopen(myargs->clisockh, "w+");
	if (!myargs->clicon) {
		PRSYSERR(errno, "Failed opening CLI connection prompt");
		goto done;
	}

	while (1) {
		ret = 0;
		alloclen = 0;
		readlen = 0;
		myargs->usrtoks = NULL;

		PRCLI(myargs->clicon, "%s", prompt);

		readlen = getline(&myargs->usrtoks, &alloclen, myargs->clicon);

		pthread_mutex_lock(&clitimerlock);
		pthread_cond_signal(&cliactive);
		pthread_mutex_unlock(&clitimerlock);

		if (0 > readlen) {
			/* some error occured, TODO: handle error */
			if (myargs->usrtoks) {
				/* Despite the error, still have to free getline allocated ptr, see doc */
				free(myargs->usrtoks);
			}
			continue;
		}
		/* User input was empty and or only contains '\n\r' i.e: Enter pressed without anything else */
		if (2 >= readlen) {
			if (myargs->usrtoks) {
				/* Still have to free getline allocated ptr, see doc */
				free(myargs->usrtoks);
			}
			continue;
		}
		/* Get rid of '\n\r' by terminating with '\0' */
		myargs->usrtoks[readlen - 2] = '\0';

		ret = dbgcli_handler(myargs);

		if (myargs->usrtoks) {
			free(myargs->usrtoks);
		}
		if (0 > ret) {
			PRCLI(myargs->clicon, "Exiting CLI\n");
			PRDEBUG("Disconnecting %s:%u\n",
				inet_ntoa(myargs->cliaddr.sin_addr),
				(unsigned)ntohs(myargs->cliaddr.sin_port) );
			break;
		}
		if (ret > 0) {
			/* Some error. TODO, handle error */
		}
	}
	fclose(myargs->clicon);
done:
	close(myargs->clisockh);
	cliprocessing = 0;
	pthread_mutex_lock(&clitimerlock);
	pthread_cond_signal(&cliactive);
	pthread_mutex_unlock(&clitimerlock);
	pthread_exit(0);
}

int
dbgcli_init(void)
{
	int ret;
	ret = pthread_create(&dbgcli_tid,
			     NULL,
			     dbgcli_thr,
			     NULL);
	if (ret) {
		PRSYSERR(errno, "Failed creating CLI thread");
		return -1;
	}
	if ( pthread_setname_np(dbgcli_tid, "dbgcli") ) {
		PRSYSERR(errno, "Failed setting CLI thread name");
	}
	return 0;
}

void
dbgcli_deinit(void)
{
	void *ret;
	pthread_cancel(dbgcli_tid);
	pthread_cancel(clitimer_tid);
	pthread_cancel(cliprocessor_tid);
	pthread_join(dbgcli_tid, &ret);
	pthread_join(clitimer_tid, &ret);
	pthread_join(cliprocessor_tid, &ret);

	pthread_cond_destroy(&cliactive);
	pthread_mutex_destroy(&clitimerlock);
	pthread_mutex_destroy(&clilock);
	PRDEBUG("Deinitializing CLI\n");
}
