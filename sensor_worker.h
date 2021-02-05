#pragma once

/* supported sensor types */
#define	INERTIA_SENS				0x060
#define	VISION_SENS				0x070
#define	TOUCH_SENS				0x080
#define	TILT_SENS				0x090
#define	GPS_SENS				0x0A0
#define	SPEED_SENS				0x0B0
#define	AMP_SENS				0x0C0
#define	VOLTAG_SENS				0x0D0
#define	SONAR_SENS				0x0E0
#define	IR_SENS					0x0F0

int sens_init(void);
void sens_deinit(void);

typedef struct {
	char *name;
	pthread_t tid;
	int tnum;
	int type;
	void* (*func)(void*);
} sens_t;
