#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "sensor_mgmt.h"
#include "sensor_worker.h"
#include "sensor_cli.h"

/*
 * multiple ways to see threads details:
 * * pstree -p `pidof sensormgmt`
 * * ps -aeT | grep sensormgmt
 * * ps -L -o pid,lwp,pri,psr,nice,start,stat,bsdtime,cmd,comm -C sensormgmt
 */

worker_t workers[MAXWORKERS];
pthread_t dbg_cli_thr;

static void setup_sigs(void);
static int create_workers(void);
static int create_cli_thr(void);

static void
setup_sigs(void)
{
	// TBD
}

static int
create_worker_thrs(void)
{
	int i, ret;
	for (i = 0; i < MAXWORKERS; i++){
		printf("creating thread %d\n", i);
		workers[i].tnum = i;
		ret = pthread_create(&workers[i].t,
									NULL,
									sensor_worker,
									(void*)(&workers[i]));
		if (ret) {
			printf("ERROR: failed creating thread %d, ret: %d\n", i, ret);
			exit(-1);
		} else {
			printf("thread %d created, ret: %d\n", i, ret);
		}
	}
	pthread_exit(NULL);
	return 0;
}

static int
create_cli_thr(void)
{
	int ret;
	ret = pthread_create(&dbg_cli_thr,
								NULL,
								sensor_dbgcli_thr,
								NULL);
	if (ret) {
			printf("ERROR: failed creating dbg cli, ret: %d\n", ret);
			exit(-1);
		} else {
			printf("dbg cli created\n");
		}

	return 0;
}

int
main(int argc, char **argv)
{
	setup_sigs();
	create_cli_thr();
	create_worker_thrs();

	return 0;
}
