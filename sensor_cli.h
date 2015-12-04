#pragma once

#define DBGCLIPORT		6000
#define SIMULMAXCONN		1

typedef struct {
	char *cmd;
	int (*func)(void*);
} clicmds;

void dbgcli_init(void);
void dbgcli_deinit(void);
