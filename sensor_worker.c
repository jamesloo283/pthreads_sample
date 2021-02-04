#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include "sensor_worker.h"
#include "sensor_common.h"

static void *sonar_thr(void *arg);
static void *speed_thr(void *arg);
static void *ir_thr(void *arg);
static void *gps_thr(void *arg);

static void
*sonar_thr(void *arg)
{
	sens_t *s = arg;
	while (1) {
		printf( "Sonar thread, idx: %d,"
			"T: %u, TID: %d, PID: %d\n",
			s->tnum,
			(u_int32_t)s->tid,
			GETTID(),
			getpid() );
		sleep(10);
	}
	pthread_exit(0);
}

static void
*speed_thr(void *arg)
{
	sens_t *s = arg;
	while (1) {
		printf( "Speed thread, idx: %d,"
			"T: %u, TID: %d, PID: %d\n",
			s->tnum,
			(u_int32_t)s->tid,
			GETTID(),
			getpid() );
		sleep(12);
	}
	pthread_exit(0);
}

static void
*ir_thr(void *arg)
{
	sens_t *s = arg;
	while (1) {
		printf( "IR thread, idx: %d,"
			"T: %u, TID: %d, PID: %d\n",
			s->tnum,
			(u_int32_t)s->tid,
			GETTID(),
			getpid() );
		sleep(14);
	}
	pthread_exit(0);
}

static void
*gps_thr(void *arg)
{
	sens_t *s = arg;
	while (1) {
		printf( "GPS thread, idx: %d,"
			"T: %u, TID: %d, PID: %d\n",
			s->tnum,
			(u_int32_t)s->tid,
			GETTID(),
			getpid() );
		sleep(10);
	}
	pthread_exit(0);
}

sens_t senstab[] = {
	/*
	 * The second member t will get assigned after thread creation, for now assign 0xDEADBEEF.
	 * The third member tnum is harded coded for now, maybe move them into a table in the future.
	 */

	{ "Sonar_sens", 0xDEADBEEF, 0, SONAR_SENS, sonar_thr },
	{ "Speed_sens", 0xDEADBEEF, 1, SPEED_SENS, speed_thr },
	{ "IR_sens", 0xDEADBEEF, 2, IR_SENS, ir_thr },
	{ "GPS_sens", 0xDEADBEEF, 3, GPS_SENS, gps_thr },
};
int senscount = sizeof(senstab) / sizeof(senstab[0]);

int
sens_init(void) {
	int i;
	int err = 0;
	for (i = 0; i < senscount; ++i) {
		if ( pthread_create(&senstab[i].tid,
				    NULL,
				    senstab[i].func,
				    (void*)(&senstab[i])) ) {
			perror("pthread_create(...)");
			printf("Sensor thread: %d)\n", senstab[i].tnum);
			++err;
		} else {
			if (pthread_setname_np(senstab[i].tid,
			    senstab[i].name)) {
				perror("pthread_setname_np(sensor)");
			}
		}
	}
	return err;
}

void
sens_deinit(void) {
	void *ret;
	int i;
	for (i = 0; i < senscount; ++i) {
		pthread_cancel(senstab[i].tid);
		pthread_join(senstab[i].tid, &ret);
		printf("Sensor thread %d exit\n", senstab[i].tnum);
	}
}
