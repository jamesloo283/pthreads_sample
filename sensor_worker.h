#pragma once

void *sensor_worker(void *arg);

typedef struct {
	pthread_t t;
	int tnum;
	int type;
} worker_t;
