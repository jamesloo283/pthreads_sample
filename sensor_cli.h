#pragma once
#include <arpa/inet.h>

/* CLI inactivity timer in seconds */
#define CLIINACTIVITYSEC	120
#define DBGCLIPORT		6000
#define SIMULMAXCONN		1

typedef struct {
	int clisockh;
	FILE *clicon;
	char *usrtoks;
	struct sockaddr_in cliaddr;
} cliargs;

typedef struct {
	char *cmd;
	int (*func)(cliargs*);
} clicmds;


int dbgcli_init(void);
void dbgcli_deinit(void);
