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

static void setup_sigs(void);

static void
sigs_init(void)
{
	// TBD
}

int
main(int argc, char **argv)
{
	sigs_init();
	dbgcli_init();
	sens_init();

	/* to do: why threads complete insteading of running? */
	sens_deinit();
	dbgcli_deinit();

	/* main robotic logic */
	while (1) {
		sleep(10);
		printf("in main\n");
	}

	return 0;
}
