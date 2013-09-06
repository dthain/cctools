#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <limits.h>
#include <string.h>

#include "copy_stream.h"
#include "create_dir.h"
#include "debug.h"
#include "list.h"
#include "path.h"
#include "xxmalloc.h"

#define MAKEFLOW_PATH "makeflow"
#define MAKEFLOW_BUNDLE_FLAG "-b"

typedef enum {UNKNOWN, PYTHON} file_type;

struct dependency{
	char *original_name;
	char *final_name;
	struct dependency *parent;
	struct dependency *superparent;
	char *output_path;
	int  depth;
	file_type type;
};

char *python_extensions[2] = { "py", "pyc" };

void initialize( char *output_directory, char *input_file, struct list *d){
	pid_t pid;
	int pipefd[2];
	pipe(pipefd);

	char expanded_input[PATH_MAX];
	realpath(input_file, expanded_input);


	switch ( pid = fork() ){
	case -1:
		fprintf( stderr, "Cannot fork. Exiting...\n" );
		exit(1);
	case 0:
		/* Child process */
		close(pipefd[0]);
		dup2(pipefd[1], 1);
		close(pipefd[1]);

		char * const args[5] = { "linking makeflow" , MAKEFLOW_BUNDLE_FLAG, output_directory, expanded_input, NULL };
		execvp(MAKEFLOW_PATH, args); 
		exit(1);
	default:
		/* Parent process */
		close(pipefd[1]);
		char next;
		char *buffer = (char *) malloc(sizeof(char));
		char *original_name;
		int size = 0;
		int depth = 1;
		while (read(pipefd[0], &next, sizeof(next)) != 0){
			switch ( next ){
				case '\t':
					original_name = (char *)realloc((void *)buffer,size+1);
					*(original_name+size) = '\0';
					buffer = (char *) malloc(sizeof(char));
					size = 0;
					break;
				case '\n':
					buffer = realloc(buffer, size+1);
					*(buffer+size) = '\0';
					struct dependency *new_dependency = (struct dependency *) malloc(sizeof(struct dependency));
					new_dependency->original_name = original_name;
					new_dependency->final_name = buffer;
					new_dependency->depth = depth;
					new_dependency->parent = NULL;
					new_dependency->superparent = NULL;
					list_push_tail(d, (void *) new_dependency);
					size = 0;
					original_name = NULL;
					buffer = NULL;
					break;
				case '\0':
				default:
					buffer = realloc(buffer, size+1);
					*(buffer+size) = next;
					size++;
			}
		}
	}
}

void display_dependencies(struct list *d){
	struct dependency *dep;

	list_first_item(d);
	while((dep = list_next_item(d))){
		if(dep->parent){
			printf("%s %s %d %d %s %s %s\n", dep->original_name, dep->final_name, dep->depth, dep->type, dep->parent->final_name, dep->superparent->final_name, dep->output_path);
		} else {
			printf("%s %s %d %d n/a n/a %s\n", dep->original_name, dep->final_name, dep->depth, dep->type, dep->output_path);
		}
	}
}

file_type file_extension_known(const char *filename){
	const char *extension = path_extension(filename);

	int j;
	for(j=0; j< 2; j++){
		if(!strcmp(python_extensions[j], extension))
			return PYTHON;
	}

	return UNKNOWN;
}

file_type find_driver_for(const char *name){
	file_type type = UNKNOWN;

	if((type = file_extension_known(name))){}

	return type;
}

struct list *find_dependencies_for(struct dependency *dep){
	pid_t pid;
	int pipefd[2];
	pipe(pipefd);

	switch ( pid = fork() ){
	case -1:
		fprintf( stderr, "Cannot fork. Exiting...\n" );
		exit(1);
	case 0:
		/* Child process */
		close(pipefd[0]);
		dup2(pipefd[1], 1);
		close(pipefd[1]);
		char * const args[3] = { "locating dependencies" , dep->original_name, NULL };
		switch ( dep->type ){
			case PYTHON:
				execvp("./python_driver", args);
			case UNKNOWN:
				break;
		}
		exit(1);
	default:
		/* Parent process */
		close(pipefd[1]);
		char next;
		char *buffer = (char *) malloc(sizeof(char));
		char *original_name;
		int size = 0;
		int depth = dep->depth  + 1;
		struct list *new_deps = list_create();

		while (read(pipefd[0], &next, sizeof(next)) != 0){
			switch ( next ){
				case ' ':
					original_name = (char *)realloc((void *)buffer,size+1);
					*(original_name+size) = '\0';
					buffer = (char *) malloc(sizeof(char));
					size = 0;
					break;
				case '\n':
					buffer = realloc(buffer, size+1);
					*(buffer+size) = '\0';
					struct dependency *new_dependency = (struct dependency *) malloc(sizeof(struct dependency));
					new_dependency->original_name = original_name;
					new_dependency->final_name = buffer;
					new_dependency->depth = depth;
					new_dependency->parent = dep;
					if(dep->superparent){
						new_dependency->superparent = dep->superparent;
					} else {
						new_dependency->superparent = dep;
					}
					list_push_tail(new_deps, new_dependency);
					size = 0;
					buffer = NULL;
					break;
				default:
					buffer = realloc(buffer, size+1);
					*(buffer+size) = next;
					size++;
			}
		}
		return new_deps;
	}
}

void find_dependencies(struct list *d){
	struct dependency *dep;
	struct list *new;

	list_first_item(d);
	while((dep = list_next_item(d))){
		new = find_dependencies_for(dep);
		list_first_item(new);
		while((dep = list_next_item(new))){
			dep->type = find_driver_for(dep->original_name);
			list_push_tail(d, dep);
		}
		list_delete(new);
		new = NULL;
	}
}

void find_drivers(struct list *d){
	struct dependency *dep;
	list_first_item(d);
	while((dep = list_next_item(d))){
		dep->type = find_driver_for(dep->original_name);
	}
}

void determine_package_structure(struct list *d, char *output_dir){
	struct dependency *dep;
	list_first_item(d);
	while((dep = list_next_item(d))){
		char resolved_path[PATH_MAX];
		if(dep->parent && dep->parent->output_path){
			sprintf(resolved_path, "%s", dep->parent->output_path);
		} else {
			sprintf(resolved_path, "%s", output_dir);
		}
		switch(dep->type){
			case PYTHON:
				sprintf(resolved_path, "%s/%s", resolved_path, dep->final_name);
				break;
			default:
				/* TODO: naming conflicts */
				break;
		}
		dep->output_path = xxstrdup(resolved_path);
	}
}

void build_package(struct list *d){
	struct dependency *dep;
	list_first_item(d);
	while((dep = list_next_item(d))){
		char tmp_path[PATH_MAX];
		switch(dep->type){
			case PYTHON:
				if(!create_dir(dep->output_path, 0777)) fatal("Could not create directory.\n");
				sprintf(tmp_path, "%s/__main__.py", dep->output_path);
				copy_file_to_file(dep->final_name, tmp_path);
				break;
			default:
				sprintf(tmp_path, "%s/%s", dep->output_path, dep->final_name);
				printf("%s -> %s\n", dep->final_name, tmp_path);
				copy_file_to_file(dep->original_name, tmp_path);
				break;
		}
	}
}

int main(void){
	char *output = "output_dir";
	char *input = "test.mf";

	struct list *dependencies;
	dependencies = list_create();

	initialize(output, input, dependencies);

	find_drivers(dependencies);
	find_dependencies(dependencies);

	determine_package_structure(dependencies, output);
	build_package(dependencies);

	display_dependencies(dependencies);

	return 0;
}
