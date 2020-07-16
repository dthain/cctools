#ifndef DATASWARM_TASK_H
#define DATASWARM_TASK_H

typedef enum {
	DATASWARM_TASK_READY,
	DATASWARM_TASK_RUNNING,
	// ...
} dataswarm_task_state_t;

struct dataswarm_task {
	dataswarm_task_state_t state;
	cctools_uuid_t uuid;
	// ...
};

struct dataswarm_task * dataswarm_task_create( );

#endif

