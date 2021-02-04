#pragma once
#include <arpa/inet.h>

#define DBGCLIPORT		6000
#define SIMULMAXCONN		1

typedef struct {
	int clih;
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
