#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "sensor_cli.h"

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
	return 0;
}

static int
sen_set(void *arg) {
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
int cmdtab_size = sizeof(cmdtab) / sizeof(cmdtab[0]);

static int
sens_dbgcli_handler(FILE *cliout, char *msg, int len)
{
	int arg;
	if (0 == len || 2 == len) {
		return 0;
	}

	msg[len - 2] = '\0';

	int i;
	for (i = 0; i < cmdtab_size; ++i) {
		if (!strncmp(cmdtab[i].cmd, msg, len)) {
			return cmdtab[i].func((void*)&arg);
		}
	}

	fprintf(cliout, "Invalid command '%s'\n", msg);
	return 0;
}


static void*
sens_dbgcli_thr(void *arg)
{
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sfd) {
		perror("socket(...)");
		exit(1);
	}

	int ret;
	int optval = 1;
	/* reuse port/address */
	ret = setsockopt(sfd,
						  SOL_SOCKET,
						  //SO_REUSEPORT | SO_REUSEADDR,
						  SO_REUSEADDR,
						  &optval,
						  sizeof(optval));
	if (-1 == ret) {
		perror("setsockopt(...,SO_REUSEPORT | SO_REUSEADDR,...)");
		exit(1);
	}

	/* 10 seconds linger to allow data transfer completion */
	struct linger ling;
	memset(&ling, 0, sizeof(ling));
	ling.l_onoff = 1;
	ling.l_linger = 10;
	ret = setsockopt(sfd,
						  SOL_SOCKET,
						  SO_LINGER,
						  (const char *)&ling,
						  sizeof(ling));
	if (-1 == ret) {
		perror("setsockopt(...,SO_LINGER,...)");
		exit(1);
	}

	struct sockaddr_in srvaddr;
	memset(&srvaddr, 0, sizeof(srvaddr));
	srvaddr.sin_family = AF_INET;
	/* accept connection on any IP */
	srvaddr.sin_addr.s_addr = INADDR_ANY;
	srvaddr.sin_port = htons(DBGCLIPORT);
	ret = bind(sfd, (struct sockaddr *)&srvaddr, sizeof(srvaddr));
	if (-1 == ret) {
		perror("bind(...)");
		exit(1);
	}

	char prompt[16] = { };
	snprintf(prompt, sizeof(prompt), "%s", "Hello >");
	printf("CLI waiting for connection\n");
	while (1) {
		ret = listen(sfd, SIMULMAXCONN);
		if (-1 == ret) {
			perror("listen(...)");
			exit(1);
		}

		struct sockaddr_in cliaddr;
		socklen_t clilen = sizeof(cliaddr);
		int clih = accept(sfd, (struct sockaddr *)&cliaddr, &clilen);
		if (-1 == clih) {
			perror("accept(...)");
			exit(1);
		}

		FILE *clicon = fdopen(clih, "w+");
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
				break;
			}
		}
		fflush(clicon);
		fclose(clicon);
		close(clih);
	}
	pthread_exit(0);
}

void
dbgcli_init(void)
{
	int ret;
	ret = pthread_create(&dbg_cli_thr,
								NULL,
								sens_dbgcli_thr,
								NULL);
	if (ret) {
		printf("ERROR: creating dbg cli, ret: %d\n", ret);
		exit(-1);
	} else {
		printf("dbg cli created\n");
	}
}

void
dbgcli_deinit(void)
{
	void *ret;
	pthread_cancel(dbg_cli_thr);
	pthread_join(dbg_cli_thr, &ret);
	printf("dbgcli completed\n");
}
