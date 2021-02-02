#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "sensor_cli.h"
#include "sensor_common.h"

static pthread_t dbg_cli_thr;

static void* sens_dbgcli_thr(void *arg);
static int sens_dbgcli_handler(FILE *cliout, char *msg, int len);
static int sen_get(void *arg);
static int sen_set(void *arg);
static int cli_exit(void *arg);

static int
cli_exit(void *arg) {
	return -1;
}

static int
sen_get(void *arg) {
	cliargs *args = (cliargs*)arg;
	fprintf(args->fhd, "get called, args: %s\n", args->args);
	return 0;
}

static int
sen_set(void *arg) {
	cliargs *args = (cliargs*)arg;
	fprintf(args->fhd, "set called, args: %s\n", args->args);
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
sens_dbgcli_handler(FILE *cliout, char *msg, int len)
{
	/* msg was from cli, either empty or only contains '\n\r' */
	if (2 >= len) {
		return 0;
	}

	/* get rid of '\n\r' */
	msg[len - 2] = '\0';
	char delimiters[] = " ";
	char *cmd = strtok(msg, delimiters);
	cliargs args;
	args.fhd = cliout;
	args.args = strtok(NULL, "");

	int i;
	for (i = 0; i < cmdtab_size; ++i) {
		if (!strncmp(cmdtab[i].cmd, cmd, strlen(cmd))) {
			return cmdtab[i].func((void*)&args);
		}
	}

	/* cmd not found, send err msg to client */
	fprintf(cliout, "Invalid command '%s'\n", cmd);
	/* and return success */
	return 0;
}


static void*
sens_dbgcli_thr(void *arg)
{
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

	char prompt[16];
	snprintf(prompt, sizeof(prompt), "%s", "DBGCLI > ");
	printf("CLI thread, TID: %d,  port: %d\n", GETTID(), DBGCLIPORT);
	while (1) {
		if ( listen(sfd, SIMULMAXCONN) ) {
			perror("listen(...) error");
			exit(1);
		}

		struct sockaddr_in cliaddr;
		socklen_t clilen = sizeof(cliaddr);
		int clih;
		clih = accept(sfd, (struct sockaddr*)&cliaddr, &clilen);
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
		printf( "Connecting with %s:%u\n",
			inet_ntoa(cliaddr.sin_addr),
			(unsigned)ntohs(cliaddr.sin_port) );
		(void)fflush(stdout);

		FILE *clicon = fdopen(clih, "w+");
		if (!clicon) {
			perror("fdopen(clih) error");
			exit(1);
		}

		while (1) {
			int nrcv;
			char *rcvbuf = NULL;
			size_t len = 0;
			fprintf(clicon, "%s", prompt);
			(void)fflush(clicon);
			nrcv = getline(&rcvbuf, &len, clicon);
			ret = sens_dbgcli_handler(clicon, rcvbuf, nrcv);
			free(rcvbuf);
			if (0 > ret) {
				fprintf(clicon, "Exiting CLI...\n");
				printf("Disconnecting %s:%u\n",
					inet_ntoa(cliaddr.sin_addr),
					(unsigned)ntohs(cliaddr.sin_port) );
				(void)fflush(stdout);
				break;
			}
		}
		fflush(clicon);
		fclose(clicon);
		close(clih);
	}
	pthread_exit(0);
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
