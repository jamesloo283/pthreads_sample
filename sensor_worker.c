#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "sensor_worker.h"

/*
 * gettid(...) isn't portable and isn't implemented on every platform
 * so unfortunately, this has to be done via syscall
 */
#define GETTID() (pid_t)syscall(SYS_gettid)

static void *sonar_thr(void *arg);
static void *speed_thr(void *arg);
static void *ir_thr(void *arg);

static void
*sonar_thr(void *arg)
{
	sens_t *s = arg;
	while (1) {
		printf( "Sonar thread, idx: %d,"
			"T: %u, TID: %d, PID: %d\n",
			s->tnum,
			s->t,
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
			s->t,
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
			s->t,
			GETTID(),
			getpid() );
		sleep(14);
	}
	pthread_exit(0);
}

sens_t senstab[] = {
	/* first member t will get assigned after thread creation */
	{ "sonar_sens", 0xDEADBEEF, 0, SONAR_SENS, sonar_thr },
	{ "speed_sens", 0xDEADBEEF, 1, SPEED_SENS, speed_thr },
	{ "IR_sens", 0xDEADBEEF, 2, IR_SENS, ir_thr },
};
int senscount = sizeof(senstab) / sizeof(senstab[0]);

int
sens_init(void) {
	int i;
	int err = 0;
	for (i = 0; i < senscount; ++i) {
		if ( pthread_create(&senstab[i].t,
				    NULL,
				    senstab[i].func,
				    (void*)(&senstab[i])) ) {
			perror("pthread_create(...)\n");
			printf("Sensor thread: %d)\n", senstab[i].tnum);
			++err;
		} else {
			(void)pthread_setname_np(senstab[i].t,
						senstab[i].name);
		}
	}
	return err;
}

void
sens_deinit(void) {
	void *ret;
	int i;
	for (i = 0; i < senscount; ++i) {
		pthread_cancel(senstab[i].t);
		pthread_join(senstab[i].t, &ret);
		printf("Sensor thread %d exit\n", senstab[i].tnum);
	}
}
