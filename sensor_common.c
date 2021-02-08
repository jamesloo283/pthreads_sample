#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <pthread.h>
#include "sensor_common.h"

static pthread_mutex_t prlock = PTHREAD_MUTEX_INITIALIZER;

void LOGGER(char *r) {
	/*
	 * TO DO, create and log
	 * use system time
	 */
}

void PRDEBUG(char *fmt, ...)
{
	pthread_mutex_lock(&prlock);
	va_list ap;
	char logrecord[MAXLOGRECORDLEN] = {0};
	va_start(ap, fmt);
	snprintf(logrecord, sizeof(logrecord), "DBG: %s", fmt);
#ifdef DEBUGIT
	vprintf(logrecord, ap);
	(void)fflush(stdout);
#endif
	LOGGER(logrecord);
	va_end(ap);
	pthread_mutex_unlock(&prlock);
}

void PRSYSERR(int err, char *fmt, ...)
{
	pthread_mutex_lock(&prlock);
	va_list ap;
	char logrecord[MAXLOGRECORDLEN] = {0};
	char *errstr = strerror(err);
	va_start(ap, fmt);
	snprintf(logrecord, sizeof(logrecord), "SYSERR: %d(%s). %s", err, errstr, fmt);
#ifdef DEBUGIT
	vprintf(logrecord, ap);
	(void)fflush(stdout);
#endif
	LOGGER(logrecord);
	va_end(ap);
	pthread_mutex_unlock(&prlock);
}

void PRCLI(FILE *fhd, char *fmt, ...)
{
	assert(fhd);
	flockfile(fhd);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(fhd, fmt, ap);
	(void)fflush(fhd);
	va_end(ap);
	funlockfile(fhd);
}

void PRCLIPRETTY(FILE *fhd, char *fmt, ...)
{
	flockfile(fhd);
	va_list ap;
	char logrecord[MAXLOGRECORDLEN] = {0};
	va_start(ap, fmt);
	snprintf(logrecord, sizeof(logrecord), "   %s", fmt);
	vfprintf(fhd, logrecord, ap);
	(void)fflush(fhd);
	va_end(ap);
	funlockfile(fhd);
}
