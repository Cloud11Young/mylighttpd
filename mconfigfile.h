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

int config_parse_file(server* srv, config_t* context, const char* fn);
int config_parse_cmd(server* srv, config_t* context, const char* cmd);

int config_insert_values_global(server* srv, array* a, const config_values_t* cv, config_scope_type_t cst);
int config_insert_values_internal(server* srv, array* a, const config_values_t* cv, config_scope_type_t scope);
#endif