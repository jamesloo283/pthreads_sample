#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include "sensor_worker.h"
#include "sensor_common.h"

static void *sonar_thr(void *arg);
static void *speed_thr(void *arg);
static void *ir_thr(void *arg);
static void *gps_thr(void *arg);
static void *inertia_thr(void *arg);

static int ir_get(void *arg);
static int sonar_get(void *arg);
static int speed_get(void *arg);
static int inertia_get(void *arg);
static int gps_get(void *arg);

static int ir_set(void *arg);
static int sonar_set(void *arg);
static int speed_set(void *arg);
static int inertia_set(void *arg);
static int gps_set(void *arg);

static inertia_data_t inertia_data;
static gps_data_t gps_data;
static ir_data_t ir_data;
static sonar_data_t sonar_data;
static speed_data_t speed_data;

static int ir_get(void *arg)
{
	if (!arg) {
		return -1;
	}
	int datalen = sizeof(ir_data_t);
	memcpy(arg, &ir_data, datalen);
	return datalen;
}

static int sonar_get(void *arg)
{
	if (!arg) {
		return -1;
	}
	int datalen = sizeof(sonar_data_t);
	memcpy(arg, &sonar_data, datalen);
	return datalen;
}

static int speed_get(void *arg)
{
	if (!arg) {
		return -1;
	}
	int datalen = sizeof(speed_data_t);
	memcpy(arg, &speed_data, datalen);
	return datalen;
}

static int inertia_get(void *arg)
{
	if (!arg) {
		return -1;
	}
	int datalen = sizeof(inertia_data_t);
	memcpy(arg, &inertia_data, datalen);
	return datalen;
}

static int gps_get(void *arg)
{
	if (!arg) {
		return -1;
	}
	int datalen = sizeof(gps_data_t);
	memcpy(arg, &gps_data, datalen);
	return datalen;
}

static int inertia_set(void *arg)
{
	if (!arg) {
		return -1;
	}
	inertia_data_t *uarg = arg;
	inertia_data.tilt = uarg->tilt;
	return 0;
}

static int sonar_set(void *arg)
{
	if (!arg) {
		return -1;
	}
	sonar_data_t *uarg = arg;
	if (0 < uarg->distance_cm) {
		sonar_data.distance_cm = uarg->distance_cm;
	}
	if (0 < uarg->distance_in) {
		sonar_data.distance_in = uarg->distance_in;
	}
	return 0;
}

static int ir_set(void *arg)
{
	if (!arg) {
		return -1;
	}
	ir_data_t *uarg = arg;
	if (0 < uarg->distance_cm) {
		ir_data.distance_cm = uarg->distance_cm;
	}
	if (0 < uarg->distance_in) {
		ir_data.distance_in = uarg->distance_in;
	}
	return 0;
}

static int speed_set(void *arg)
{
	if (!arg) {
		return -1;
	}
	speed_data_t *uarg = arg;
	/* Rate could be negative */
	if (0 != uarg->rate) {
		speed_data.rate = uarg->rate;
	}
	if (0 < uarg->pwm) {
		speed_data.pwm = uarg->pwm;
	}
	if (0 < uarg->rpm) {
		speed_data.rpm = uarg->rpm;
	}
	if (0 < uarg->cmpm) {
		speed_data.cmpm = uarg->cmpm;
	}
	if (0 < uarg->inpm) {
		speed_data.inpm = uarg->inpm;
	}
	return 0;
}

static int gps_set(void *arg)
{
	if (!arg) {
		return -1;
	}
	gps_data_t *uarg = arg;
	gps_data.heading = uarg->heading;
	gps_data.degree = uarg->degree;
	gps_data.minute = uarg->minute;
	gps_data.seconds = uarg->seconds;
	gps_data.elevation = uarg->elevation;
	return 0;
}

sens_t senstab[] = {
	/*
	 * The second member t will get assigned after thread creation, for now assign 0xDEADBEEF.
	 * The third member tnum is harded coded for now, maybe move them into a table in the future.
	 */
	{ "sonar", 0xDEADBEEF, 0, SONAR_SENS, sonar_thr, &sonar_get, &sonar_set, &sonar_data },
	{ "speed", 0xDEADBEEF, 1, SPEED_SENS, speed_thr, &speed_get, &speed_set, &speed_data },
	{ "ir", 0xDEADBEEF, 2, IR_SENS, ir_thr, &ir_get, &ir_set, &ir_data },
	{ "gps", 0xDEADBEEF, 3, GPS_SENS, gps_thr, &gps_get, &gps_set, &gps_data },
	{ "inertia", 0xDEADBEEF, 4, INERTIA_SENS, inertia_thr, &inertia_get, &inertia_set, &inertia_data },
};
int senscount = sizeof(senstab) / sizeof(senstab[0]);

int get_sens_data(char *sen, void *data)
{
	int i;
	int ret = 0;
	int usenlen = (int)strlen(sen);
	int senlen;

	for (i = 0; i < senscount; ++i) {
		senlen = strlen(senstab[i].name);
		if (usenlen != senlen) {
			continue;
		}
		if (!strncmp(sen, senstab[i].name, senlen)) {
			break;
		}
	}

	if (i == senscount) {
		return -1;
	}

	if (!senstab[i].get) {
		return -1;
	}

	return senstab[i].get(data);
}

static void
*sonar_thr(void *arg)
{
	sens_t *s = arg;
	sonar_data_t *sdata = (sonar_data_t*)s->data;
	sdata->distance_cm = 0;
	sdata->distance_in = 0;
	time_t t;
	int wait = 0;
	while (1) {
#ifdef SIMULATE
		/* Randomly simulate sonar distances for testing */
		u_int32_t seedp = (unsigned)time(&t);
		rand_r(&seedp);
		/* Up to 10ft/304cm */
		sdata->distance_in = (u_int32_t)(rand() % 120);
		sdata->distance_cm = (u_int32_t)(sdata->distance_in * 2.54);
#else
		/* TO DO: sonar sensor actual read */
#endif
		/* About 20ms frequency, assuming sonar can be read about 50 times per second */
		usleep(20000);
		/* For debug, wait up to 5s (20ms x 250) for debug output */
		/* These are for simulation/debug only. Actual number should be used for real devices */
		if (wait == 0 || wait == 250) {
			PRDEBUG( "Sonar thread, idx: %d,"
				"T: %u, TID: %d, PID: %d\n",
				s->tnum,
				(u_int32_t)s->tid,
				GETTID(),
				getpid() );
			wait = 0;
		}
		++wait;
	}
	pthread_exit(0);
}

static void
*speed_thr(void *arg)
{
	sens_t *s = arg;
	while (1) {
		PRDEBUG( "Speed thread, idx: %d,"
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
	ir_data_t *irdata = (ir_data_t*)s->data;
	irdata->distance_cm = 0;
	irdata->distance_in = 0;
	time_t t;
	int wait = 0;
	while (1) {
#ifdef SIMULATE
		/* Randomly simulate ir distances for testing */
		u_int32_t seedp = (unsigned)time(&t);
		rand_r(&seedp);
		/* Up to 10ft/304cm */
		irdata->distance_in = (u_int32_t)(rand() % 120);
		irdata->distance_cm = (u_int32_t)(irdata->distance_in * 2.54);
#else
		/* TO DO: ir sensor actual read */
#endif
		/* About 10ms frequency, assuming ir can be read about 100 times per second */
		usleep(10000);
		/* For debug, wait up to 7s (10ms x 700) for debug output */
		/* These are for simulation/debug only. Actual number should be used for real devices */
		if (wait == 0 || wait == 700) {
			PRDEBUG( "IR thread, idx: %d,"
				"T: %u, TID: %d, PID: %d\n",
				s->tnum,
				(u_int32_t)s->tid,
				GETTID(),
				getpid() );
			wait = 0;
		}
		++wait;
	}
	pthread_exit(0);
}

static void
*inertia_thr(void *arg)
{
	sens_t *s = arg;
	inertia_data_t *inertia_data = (inertia_data_t*)s->data;
	inertia_data->tilt = 0;
	time_t t;
	int wait = 0;
	while (1) {
#ifdef SIMULATE
		/* Randomly simulate ir distances for testing */
		u_int32_t seedp = (unsigned)time(&t);
		rand_r(&seedp);
		/* 0 to 360 degrees */
		inertia_data->tilt = (int32_t)(rand() % 360);
#else
		/* TO DO: inertia sensor actual read */
#endif
		/* About 10ms frequency, assuming inertia can be read about 100 times per second */
		usleep(10000);
		/* For debug, wait up to 4s (10ms x 400) for debug output */
		/* These are for simulation/debug only. Actual number should be used for real devices */
		if (wait == 0 || wait == 400) {
			PRDEBUG( "Inertia thread, idx: %d,"
				"T: %u, TID: %d, PID: %d\n",
				s->tnum,
				(u_int32_t)s->tid,
				GETTID(),
				getpid() );
			wait = 0;
		}
		++wait;
	}
	pthread_exit(0);
}

static void
*gps_thr(void *arg)
{
	sens_t *s = arg;
	while (1) {
		PRDEBUG( "GPS thread, idx: %d,"
			"T: %u, TID: %d, PID: %d\n",
			s->tnum,
			(u_int32_t)s->tid,
			GETTID(),
			getpid() );
		sleep(10);
	}
	pthread_exit(0);
}


int
sens_init(void) {
	int i;
	int err = 0;
	for (i = 0; i < senscount; ++i) {
		if ( pthread_create(&senstab[i].tid,
				    NULL,
				    senstab[i].sensthr,
				    (void*)(&senstab[i])) ) {
			PRSYSERR(errno, "Failed calling pthread_create(%s)", senstab[i].sensthr);
			PRDEBUG("Sensor thread: %d)\n", senstab[i].tnum);
			++err;
		} else {
			if (pthread_setname_np(senstab[i].tid,
			    senstab[i].name)) {
				PRSYSERR(errno, "Failed calling pthread_setname_np(%s)", senstab[i].name);
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
		PRDEBUG("Sensor thread %d, %s exiting\n", senstab[i].tnum, senstab[i].name);
	}
}
