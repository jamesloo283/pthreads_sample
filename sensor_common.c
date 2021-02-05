#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "sensor_common.h"

void LOGGER(char *r) {
	/*
	 * TO DO, create and log
	 * use system time
	 */
}

void PRDEBUG(char *fmt, ...)
{
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
}

void PRSYSERR(int err, char *fmt, ...)
{
	va_list ap;
	char logrecord[MAXLOGRECORDLEN] = {0};
	char *errstr = strerror(err);
	va_start(ap, fmt);
	snprintf(logrecord, sizeof(logrecord), "SYSERR: %d-%s, %s", err, errstr, fmt);
#ifdef DEBUGIT
	vprintf(logrecord, ap);
	(void)fflush(stdout);
#endif
	LOGGER(logrecord);
	va_end(ap);
}

void PRCLI(FILE *fhd, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(fhd, fmt, ap);
	va_end(ap);
	(void)fflush(fhd);
}
