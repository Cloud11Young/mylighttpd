#include "mplugin.h"

typedef struct plugin_load_functions{
	const char* name;
	int (*plugin_init)(plugin* p);
}plugin_load_functions;

static plugin_load_functions load_functions[] = {
#define PLUGIN_INIT(x)\
	{#x,&x##_plugin_init},

#include "mplugins_static.h"
	
	{NULL,NULL}
#undef PLUGIN_INIT(x)
}

int plugins_load(server* srv){
	plugin* p;
	int i, j;

	for (i = 0; i < srv->srvconf.modules->used; i++){
		data_string* ds = (data_string*)srv->srvconf.modules->data[i];
		const char* module = ds->value->ptr;

		for (j = 0; j < i; j++){
			if (buffer_is_equal(ds->value, srv->srvconf.modules->data[j]->value)){
				log_error_write(srv, __FILE__, __LINE__, "sbs",
					"Can not load plugin ", ds->value,
					"more than once, please fix your config");
				continue;
			}
		}

		for (j = 0; load_functions[j].name; j++){
			if (strcmp(load_functions[j].name, module) == 0){
				p = plugin_init();
				if ((*load_functions[j].plugin_init)(p)){
					log_error_write(srv, __FILE__, __LINE__, "ss",
						module, " plugin init failed");
					plugin_free(p);
					return -1;
				}
				plugins_register(srv, p);
				break;
			}
		}

		if (!load_functions[j].name){
			log_error_write(srv, __FILE__, __LINE__, "ss", module, " not found");
			return -1;
		}
	}
	return 0;
}

void plugins_free(server* srv){

}

int plugins_call_init(server* srv){

}

handler_t plugins_call_set_defaults(server* srv){

}
