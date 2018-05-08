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

void* configparserAlloc(void* (*mallocproc)(size_t));
void configparser(void* yyp, int yymajor, buffer* yyminor, config_t* ctx);

#endif