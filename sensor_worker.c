#include <stdlib.h>
#include <stdio.h>
#include "sensor_worker.h"

void
*sensor_worker(void *arg)
{
	worker_t *w = arg;
	sleep(10);
	printf("thread num: %d\n", w->tnum);
	sleep(10);
}
