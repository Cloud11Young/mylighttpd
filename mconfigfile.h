#ifndef MCONFIGFILE_H_
#define MCONFIGFILE_H_

#include "mbase.h"
#include "marray.h"

typedef struct {
	server* srv;
	int ok;
	array* all_configs;
	vector_config_weak configs_stack;
	data_config* current;
	buffer* basedir;
}config_t;

#endif