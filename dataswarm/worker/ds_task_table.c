#include "ds_task_table.h"
#include "ds_worker.h"
#include "ds_process.h"
#include "common/ds_task.h"
#include "common/ds_message.h"

#include "hash_table.h"
#include "jx.h"
#include "stringtools.h"
#include "debug.h"
#include "delete_dir.h"
#include "macros.h"

#include <dirent.h>
#include <string.h>

/*
Every time a task changes state, record the change on disk,
and then send an async update message if requested.
*/

static void update_task_state( struct ds_worker *w, struct ds_task *task, ds_task_state_t state, int send_update_message )
{
	task->state = state;

	char * task_meta = ds_worker_task_meta(w,task->taskid);
	ds_task_to_file(task,task_meta);
	free(task_meta);

	if(send_update_message) {
		struct jx *msg = ds_message_task_update( task->taskid, ds_task_state_string(state) );
		ds_json_send(w->manager_link,msg,time(0)+w->long_timeout);
		free(msg);
	}
}

ds_result_t ds_task_table_submit( struct ds_worker *w, const char *taskid, struct jx *jtask )
{
	struct ds_task *task = ds_task_create(jtask);
	if(task) {
		hash_table_insert(w->task_table, taskid, task);
		return DS_RESULT_SUCCESS;
	} else {
		return DS_RESULT_BAD_PARAMS;
	}
}

ds_result_t ds_task_table_get( struct ds_worker *w, const char *taskid, struct jx **jtask )
{
	struct ds_task *task = hash_table_lookup(w->task_table, taskid);
	if(task) {
		*jtask = ds_task_to_jx(task);
		return DS_RESULT_SUCCESS;
	} else {
		return DS_RESULT_NO_SUCH_TASKID;
	}
}

ds_result_t ds_task_table_remove( struct ds_worker *w, const char *taskid )
{
	struct ds_task *task = hash_table_lookup(w->task_table, taskid);
	if(task) {
		update_task_state(w,task,DS_TASK_DELETING,0);
		return DS_RESULT_SUCCESS;
	} else {
		return DS_RESULT_NO_SUCH_TASKID;
	}
}

ds_result_t ds_task_table_list( struct ds_worker *w, struct jx **result )
{
	struct ds_task *task;
	char *taskid;

	*result = jx_object(0);

	hash_table_firstkey(w->task_table);
	while(hash_table_nextkey(w->task_table,&taskid,(void**)&task)) {
		jx_insert(*result,jx_string(taskid),ds_task_to_jx(task));
	}

	return DS_RESULT_SUCCESS;	
}

int ds_task_resources_avail( struct ds_worker *w, struct ds_task *t )
{
	return t->resources->cores+w->cores_inuse <= w->cores_total
		&& t->resources->memory + w->memory_inuse <= w->memory_total
		&& t->resources->disk + w->disk_inuse <= w->disk_total;
}

void ds_task_resources_alloc( struct ds_worker *w, struct ds_task *t )
{
	w->cores_inuse  += t->resources->cores;
	w->memory_inuse += t->resources->memory;
	w->disk_inuse   += t->resources->disk;
}

void ds_task_resources_free( struct ds_worker *w, struct ds_task *t )
{
	w->cores_inuse  -= t->resources->cores;
	w->memory_inuse -= t->resources->memory;
}

void ds_task_disk_free( struct ds_worker *w, struct ds_task *t )
{
	w->disk_inuse -= t->resources->disk;
}

/*
Consider each task currently in possession of the worker,
and act according to it's current state.
*/

void ds_task_table_advance( struct ds_worker *w )
{
	struct ds_task *task;
	struct ds_process *process;
	char *taskid;

	hash_table_firstkey(w->task_table);
	while(hash_table_nextkey(w->task_table,&taskid,(void**)&task)) {

		switch(task->state) {
			case DS_TASK_READY:
				if(!ds_task_resources_avail(w,task)) break;

				process = ds_process_create(task,w);
				if(process) {
					hash_table_insert(w->process_table,taskid,process);
					// XXX check for invalid mounts?
					if(ds_process_start(process,w)) {
						ds_task_resources_alloc(w,task);
						update_task_state(w,task,DS_TASK_RUNNING,1);
					} else {
						update_task_state(w,task,DS_TASK_FAILED,1);
					}
				} else {
					update_task_state(w,task,DS_TASK_FAILED,1);
				}
				break;
			case DS_TASK_RUNNING:
				process = hash_table_lookup(w->process_table,taskid);
				if(ds_process_isdone(process)) {
					ds_task_resources_free(w,task);
					update_task_state(w,task,DS_TASK_DONE,1);
				}
				break;
			case DS_TASK_DONE:
				// Do nothing until removed.
				break;
			case DS_TASK_FAILED:
				// Do nothing until removed.
				break;
			case DS_TASK_DELETING:
				{
				char *sandbox_dir = ds_worker_task_sandbox(w,task->taskid);
				char *task_dir = ds_worker_task_dir(w,task->taskid);

				// First delete the sandbox dir, which could be large and slow.
				delete_dir(sandbox_dir);

				// Now delete the task dir and metadata file, which should be quick.
				delete_dir(task_dir);

				free(sandbox_dir);
				free(task_dir);
			  
				// Send the deleted message (need the task structure still)
				update_task_state(w,task,DS_TASK_DELETED,1);

				// Now note that the storage has been reclaimed.
				ds_task_disk_free(w,task);

				// Remove and delete the process and task structures.
				process = hash_table_remove(w->process_table,taskid);
				if(process) ds_process_delete(process);
				task = hash_table_remove(w->task_table,taskid);
				if(task) ds_task_delete(task);
				}
				break;
			case DS_TASK_DELETED:
				break;
		}
	}

}

/*
After a restart, scan the tasks on disk to recover the table,
then cancel any tasks that were running and are now presumed dead.
Note that we are not connected to the master at this point,
so do not send any message.  A complete set up updates gets sent
when we reconnect.
*/

void ds_task_table_recover( struct ds_worker *w )
{
	char * task_dir = string_format("%s/task",w->workspace);

	DIR *dir;
	struct dirent *d;

	debug(D_DATASWARM,"checking %s for tasks to recover...",task_dir);

	dir = opendir(task_dir);
	if(!dir) return;

	while((d=readdir(dir))) {
		if(!strcmp(d->d_name,".")) continue;
		if(!strcmp(d->d_name,"..")) continue;

		char * task_meta;
		struct ds_task *task;

		debug(D_DATASWARM,"recovering task %s",d->d_name);

		task_meta = ds_worker_task_meta(w,d->d_name);

		task = ds_task_create_from_file(task_meta);
		if(task) {
			hash_table_insert(w->task_table,task->taskid,task);
			if(task->state==DS_TASK_RUNNING) {
				update_task_state(w,task,DS_TASK_FAILED,0);
			}
			// Note that deleted tasks will be handled in task_advance.
		}
		free(task_meta);

	}

	debug(D_DATASWARM,"done recovering tasks");
	closedir(dir);
	free(task_dir);
}
