
#include "makeflow_local_resources.h"

#include "host_disk_info.h"
#include "host_memory_info.h"
#include "load_average.h"
#include "macros.h"
#include "debug.h"

void makeflow_local_resources_print( struct rmsummary *r )
{
	printf("local resources: %" PRId64 " cores, %" PRId64 " MB memory, %" PRId64 " MB disk\n",r->cores,r->memory,r->disk);
}

void makeflow_local_resources_debug( struct rmsummary *r )
{
	debug(D_MAKEFLOW,"local resources: %" PRId64 " cores, %" PRId64 " MB memory, %" PRId64 " MB disk\n",r->cores,r->memory,r->disk);
}

void makeflow_local_resources_measure( struct rmsummary *r )
{
	UINT64_T avail, total;

	r->cores = load_average_get_cpus();

	host_memory_info_get(&avail,&total);
	r->memory = total / MEGA;

	host_disk_info_get(".",&avail,&total);
	r->disk = avail / MEGA;
}

int  makeflow_local_resources_available(struct rmsummary *local, const struct rmsummary *resources_asked)
{
	const struct rmsummary *s = resources_asked;
	return s->cores<=local->cores && s->memory<=local->memory && s->disk<=local->disk;
}

void makeflow_local_resources_subtract( struct rmsummary *local, struct dag_node *n )
{
	const struct rmsummary *s = n->resources_allocated;
	if(s->cores>=0)  local->cores -= s->cores;
	if(s->memory>=0) local->memory -= s->memory;		
	if(s->disk>=0)   local->disk -= s->disk;
	makeflow_local_resources_debug(local);
}

void makeflow_local_resources_add( struct rmsummary *local, struct dag_node *n )
{
	const struct rmsummary *s = n->resources_allocated;
	if(s->cores>=0)  local->cores += s->cores;
	if(s->memory>=0) local->memory += s->memory;		
	if(s->disk>=0)   local->disk += s->disk;
	makeflow_local_resources_debug(local);
}


