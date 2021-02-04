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
#include "sensor_cli.h"
#include "sensor_common.h"

static int cliprocessing = 0;
static pthread_mutex_t clilock;

static pthread_t dbg_cli_thr;

static void* sens_dbgcli_thr(void *arg);
static void* sens_dbgcli_processor_thr(void *arg);
static int sens_dbgcli_handler(cliargs *args);
static int sen_get(cliargs *arg);
static int sen_set(cliargs *arg);
static int cli_exit(cliargs *arg);

static int
cli_exit(cliargs *arg) {
	return -1;
}

static int
sen_get(cliargs *arg) {
	if (!arg) {
		printf("sen_get: invalid args\n");
		return 0;
	}
	if (!arg->usrtoks) {
		fprintf(arg->clicon, "sen_get: invalid args\n");
		return 0;
	}

	fprintf(arg->clicon, "sen_get called with args: %s\n", arg->usrtoks);
	return 0;
}

static int
sen_set(cliargs *arg) {
	if (!arg) {
		printf("sen_set: invalid args\n");
		return 0;
	}
	if (!arg->usrtoks) {
		fprintf(arg->clicon, "sen_set: invalid args\n");
		return 0;
	}

	fprintf(arg->clicon, "sen_set called with args: %s\n", arg->usrtoks);
	return 0;
}

clicmds cmdtab[] = {
	{ "set", sen_set },
	{ "get", sen_get },
	{ "exit", cli_exit },
	{ "quit", cli_exit },
	{ "bye", cli_exit },
	{ "q", cli_exit },
};
static int cmdtab_size = sizeof(cmdtab) / sizeof(cmdtab[0]);

static int
sens_dbgcli_handler(cliargs *args)
{
	int len = strlen(args->usrtoks);
	if (0 >= len) {
		return 0;
	}

	char *worktoks = malloc(len);
	if (!worktoks) {
		fprintf(args->clicon, "CLI internal error, aborting\n");
		return 0;
	}
	strncpy(worktoks, args->usrtoks, len);
	char delimiters[] = " ";
	char *cmd = strtok(worktoks, delimiters);
	int i;
	for (i = 0; i < cmdtab_size; ++i) {
		if (!strncmp(cmdtab[i].cmd, cmd, strlen(cmdtab[i].cmd))) {
			break;
		}
	}
	int ret = 0;
	if (i == cmdtab_size) {
		/* cmd not found, send err msg to CLI prompt */
		fprintf(args->clicon, "Invalid command '%s'\n", cmd);
	} else {
		ret = cmdtab[i].func(args);
	}
	if (worktoks) {
		free(worktoks);
	}
	return ret;
}


static void*
sens_dbgcli_thr(void *arg)
{
	pthread_mutex_init(&clilock, NULL);
	/* IPv4 domain, reliable full-duplex stream type, TCP protocol */
	int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (-1 == sfd) {
		perror("socket(...) error");
		exit(1);
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
		perror("setsockopt( SO_REUSEADDR ) error");
		exit(1);
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
		perror("setsockopt( SO_LINGER ) error");
		exit(1);
	}

	struct sockaddr_in srvaddr;
	memset(&srvaddr, 0, sizeof(srvaddr));
	srvaddr.sin_family = AF_INET;
	/* accept connection on any IP */
	srvaddr.sin_addr.s_addr = INADDR_ANY;
	srvaddr.sin_port = htons(DBGCLIPORT);
	if ( bind(sfd, (struct sockaddr *)&srvaddr, sizeof(srvaddr)) ) {
		perror("bind(...) error ");
		exit(1);
	}

	printf("CLI thread, TID: %d,  port: %d\n", GETTID(), DBGCLIPORT);
	int clih;
	FILE *clicon;
	cliargs myargs;
	while (1) {
		if ( listen(sfd, SIMULMAXCONN) ) {
			perror("listen(...) error");
			exit(1);
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
				perror("accept(...) error");
				exit(1);
				break;
			}
		}

		pthread_mutex_lock(&clilock);
		if (cliprocessing == 1) {
			clicon = fdopen(clih, "w+");
			if (!clicon) {
				perror("fdopen(clih) error");
			} else {
				fprintf(clicon, "A CLI session is already active, aborting this connection\n");
				(void)fflush(clicon);
				fclose(clicon);
				close(clih);
			}
			pthread_mutex_unlock(&clilock);
			continue;
		}
		cliprocessing = 1;
		pthread_mutex_unlock(&clilock);

		myargs.clih = clih;
		printf( "Connecting with %s:%u\n",
			inet_ntoa(myargs.cliaddr.sin_addr),
			(unsigned)ntohs(myargs.cliaddr.sin_port) );
		(void)fflush(stdout);
		pthread_t cli_processor;
		ret = pthread_create(&cli_processor,
				     NULL,
				     sens_dbgcli_processor_thr,
				     (void*)&myargs);
		if (ret) {
			perror("pthread_create( CLI PROCESSOR ) error");
		} else {
			if ( pthread_setname_np(cli_processor, "cliprocessor") ) {
				perror("pthread_setname_np( DBG CLI ) error");
			}
		}
	}
	pthread_mutex_destroy(&clilock);
	pthread_exit(0);
}

static void*
sens_dbgcli_processor_thr(void *args) {
	cliargs *myargs = (cliargs*)args;
	if (-1 == myargs->clih) {
		return NULL;
	}
	int ret;
	size_t alloclen, readlen;
	char prompt[16];
	snprintf(prompt, sizeof(prompt), "%s", "DBGCLI > ");

	myargs->clicon = fdopen(myargs->clih, "w+");
	if (!myargs->clicon) {
		close(myargs->clih);
		cliprocessing = 0;
		perror("fdopen(myargs->clih) error");
		return NULL;
	}

	while (1) {
		ret = 0;
		alloclen = 0;
		readlen = 0;
		myargs->usrtoks = NULL;
		fprintf(myargs->clicon, "%s", prompt);
		(void)fflush(myargs->clicon);

		readlen = getline(&myargs->usrtoks, &alloclen, myargs->clicon);
		if (0 > readlen) {
			/* some error occured */
			/* TODO: handle error */
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

		ret = sens_dbgcli_handler(myargs);
		if (myargs->usrtoks) {
			free(myargs->usrtoks);
		}
		if (0 > ret) {
			fprintf(myargs->clicon, "Exiting CLI...\n");
			printf("Disconnecting %s:%u\n",
				inet_ntoa(myargs->cliaddr.sin_addr),
				(unsigned)ntohs(myargs->cliaddr.sin_port) );
			(void)fflush(stdout);
			break;
		}
	}
	(void)fflush(myargs->clicon);
	fclose(myargs->clicon);
	close(myargs->clih);
	cliprocessing = 0;
}

int
dbgcli_init(void)
{
	int ret;
	ret = pthread_create(&dbg_cli_thr,
			     NULL,
			     sens_dbgcli_thr,
			     NULL);
	if (ret) {
		perror("pthread_create( DBG CLI ) error");
		return -1;
	}
	if ( pthread_setname_np(dbg_cli_thr, "dbgcli") ) {
		perror("pthread_setname_np( DBG CLI ) error");
	}
	return 0;
}

void
dbgcli_deinit(void)
{
	void *ret;
	pthread_cancel(dbg_cli_thr);
	pthread_join(dbg_cli_thr, &ret);
	printf("DBG CLI exit\n");
}
