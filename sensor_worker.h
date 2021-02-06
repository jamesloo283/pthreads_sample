#pragma once

/* supported sensor types */
#define	INERTIA_SENS				0x50
#define	VISION_SENS				0x51
#define	TOUCH_SENS				0x52
#define	TILT_SENS				0x53
#define	GPS_SENS				0x54
#define	SPEED_SENS				0x55
#define	AMP_SENS				0x56
#define	VOLTAG_SENS				0x57
#define	SONAR_SENS				0x58
#define	IR_SENS					0x58

int sens_init(void);
void sens_deinit(void);
int get_sens_data(char *sen, void *data);

typedef struct {
	char *name;
	pthread_t tid;
	int tnum;
	int type;
	void* (*sensthr)(void*);
	int (*get)(void*);
	int (*set)(void*);
	void* data;
} sens_t;

typedef struct {
	u_int32_t heading;
	int32_t degree;
	int32_t minute;
	int32_t seconds;
	int32_t elevation;
	/* TBD */
} gps_data_t;

typedef struct {
	int32_t tilt;
	/* TBD */
} inertia_data_t;

typedef struct {
	u_int32_t distance_cm;
	u_int32_t distance_in;
	/* TBD */
} ir_data_t;

typedef struct {
	u_int32_t distance_cm;
	u_int32_t distance_in;
	/* TBD */
} sonar_data_t;

typedef struct {
	int32_t rate;
	u_int32_t pwm;
	u_int32_t rpm;
	u_int32_t cmpm;
	u_int32_t inpm;
	/* TBD */
} speed_data_t;
