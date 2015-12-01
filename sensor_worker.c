#include <stdlib.h>
#include <stdio.h>
#include "sensor_worker.h"

static void *sonar_thr(void *arg);
static void *speed_thr(void *arg);
static void *ir_thr(void *arg);

static void
*sonar_thr(void *arg)
{
	sens_t *s = arg;
	while (1) {
		printf("I'm a sonar sensor, idx: %d, t: %li\n",
				s->tnum,
				(unsigned long int)s->t);
		sleep(10);
	}
	pthread_exit(0);
}

static void
*speed_thr(void *arg)
{
	sens_t *s = arg;
	while (1) {
		printf("I'm a speed sensor, id: %d, t: %li\n",
				s->tnum,
				(unsigned long int)s->t);
		sleep(12);
	}
	pthread_exit(0);
}

static void
*ir_thr(void *arg)
{
	sens_t *s = arg;
	while (1) {
		printf("I'm IR sensor, id: %d, t: %li\n",
				s->tnum,
				(unsigned long int)s->t);
		sleep(15);
	}
	pthread_exit(0);
}

sens_t senstab[] = {
	{ 0xFEFEFEFE, 0, SONAR_SENS, sonar_thr },
	{ 0xFEFEFEFE, 1, SPEED_SENS, speed_thr },
	{ 0xFEFEFEFE, 2, IR_SENS, ir_thr },
};
int senscount = sizeof(senstab) / sizeof(senstab[0]);

void
sens_init(void) {
	int i;
	int ret;
	for (i = 0; i < senscount; ++i) {
		ret = pthread_create(&senstab[i].t,
									NULL,
									senstab[i].func,
									(void*)(&senstab[i]));
		if (ret) {
			printf("ERROR creating thread %d\n", i);
		} else {
			printf("Thread %d created\n", senstab[i].tnum);
		}
	}
}

void
sens_deinit(void) {
	void *ret;
	int i;
	for (i = 0; i < senscount; ++i) {
		pthread_cancel(senstab[i].t);
		pthread_join(senstab[i].t, &ret);
		printf("Thread %d completed\n", senstab[i].tnum);
	}
}
