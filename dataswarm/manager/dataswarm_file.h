#ifndef DATASWARM_FILE_H
#define DATASWARM_FILE_H

typedef enum {
	DATASWARM_FILE_PENDING,
	DATASWARM_FILE_ALLOCATING,
	// ...
} dataswarm_file_state_t;

struct dataswarm_file {
	dataswarm_file_state_t state;
	cctools_uuid_t uuid;
	// ...
};

struct dataswarm_file * dataswarm_file_create( );

#endif

