#pragma once
#include <unistd.h>
#include <sys/syscall.h>

#define MAXLOGRECORDLEN		128
#define LOGPATH			"/tmp/sensorlogs.txt"

/*
 * gettid(...) isn't portable and isn't implemented on every platform
 * so unfortunately, this has to be done via syscall
 */
#define GETTID() (pid_t)syscall(SYS_gettid)

void LOGGER(char *r);
void PRDEBUG(char *fmt, ...);
void PRSYSERR(int err, char *fmt, ...);
void PRCLI(FILE *fhd, char *fmt, ...);
void PRCLIPRETTY(FILE *fhd, char *fmt, ...);
