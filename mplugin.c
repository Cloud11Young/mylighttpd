#include "mplugin.h"
#include <dlfcn.h>
#include <errno.h>

typedef enum{
	PLUGIN_FUNC_UNSET,
	PLUGIN_FUNC_HANDLE_URI_CLEAN,
	PLUGIN_FUNC_HANDLE_URI_RAW,
	PLUGIN_FUNC_HANDLE_REQUEST_DONE,
	PLUGIN_FUNC_HANDLE_CONNECTION_CLOSE,
	PLUGIN_FUNC_HANDLE_TRIGGER,
	PLUGIN_FUNC_HANDLE_SIGHUP,
	PLUGIN_FUNC_HANDLE_SUBREQUEST,
	PLUGIN_FUNC_HANDLE_SUBREQUEST_START,
	PLUGIN_FUNC_HANDLE_RESPONSE_START,
	PLUGIN_FUNC_HANDLE_DOCROOT,
	PLUGIN_FUNC_HANDLE_PHYSICAL,
	PLUGIN_FUNC_CONNECTION_RESET,
	PLUGIN_FUNC_INIT,
	PLUGIN_FUNC_CLEANUP,
	PLUGIN_FUNC_SET_DEFAULTS,
	PLUGIN_FUNC_SIZEOF
}plugin_t;

static plugin* plugin_init(){
	plugin* plg = calloc(1, sizeof(*plg));
	
	force_assert(plg != NULL);
	return plg;
}

static void plugin_free(plugin* p){
	int use_dlclose = 1;
	if (p->name) buffer_free(p->name);
	if (use_dlclose && p->lib){
		dlclose(p->lib);
	}
	free(p);
}

static int plugin_register(server* srv, plugin* p){
	plugin** ps;	//buffer_plugin.ptr void*
	if (0 == srv->plugins.size){
		srv->plugins.size = 4;
		srv->plugins.ptr = malloc(srv->plugins.size*sizeof(*ps));
		srv->plugins.used = 0;
	}else if (srv->plugins.size == srv->plugins.used){
		srv->plugins.size += 4;
		srv->plugins.ptr = realloc(srv->plugins.ptr, srv->plugins.size*sizeof(*ps));
	}
	ps = srv->plugins.ptr;
	ps[srv->plugins.used++] = p;
	return 0;
}

int plugins_load(server* srv){
	plugin* p;
	int i, j;
	int (* init)(plugin*);

	for (i = 0; i < srv->srvconf.modules->used; i++){
		data_string* ds = (data_string*)srv->srvconf.modules->data[i];
		const char* module = ds->value->ptr;

		for (j = 0; j < i; j++){
			if (buffer_is_equal(ds->value, ((data_string*)srv->srvconf.modules)->value)){
				log_error_write(srv, __FILE__, __LINE__, "sbs",
					"Can not load plugin ", ds->value,
					"more than once, please fix your config");
				continue;
			}
		}
		
		p = plugin_init();

		buffer_copy_buffer(srv->tmp_buf, srv->srvconf.modules_dir);
		buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN("/"));
		buffer_append_string(srv->tmp_buf, module);
		buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN(".so"));		

		if (NULL == (p->lib = dlopen(srv->tmp_buf->ptr, RTLD_NOW | RTLD_GLOBAL))){
			log_error_write(srv, __FILE__, __LINE__, "s",
				"dlopen failed ", strerror(errno));
			buffer_reset(srv->tmp_buf);
			plugin_free(p);
			return -1;
		}
		
		buffer_reset(srv->tmp_buf);
		buffer_copy_string(srv->tmp_buf, module);
		buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN("_plugin_init"));

		init=(int(*)(plugin*))dlsym(p->lib, srv->tmp_buf->ptr);
		if (init == NULL){
			const char* error = dlerror();
			if (NULL != error)
				log_error_write(srv, __FILE__, __LINE__, "ss", "dlsym: ", error);
			else
				log_error_write(srv, __FILE__, __LINE__, "ss", "dlsym symbol not found ", srv->tmp_buf->ptr);
			plugin_free(p);
			return -1;
		}
		
		if (0 != init(p)){
			log_error_write(srv, __FILE__, __LINE__, "s", module, " init failed");
			plugin_free(p);
			return -1;
		}

		plugin_register(srv, p);
	}
	return 0;
}

void plugins_free(server* srv){
	int i;
	plugins_call_cleanup(srv);

	for (i = 0; i < srv->plugins.used; i++){
		plugin* p = ((plugin**)srv->plugins.ptr)[i];
		plugin_free(p);
	}

	for (i = 0; srv->plugin_slots && i < PLUGIN_FUNC_SIZEOF; i++){
		plugin** slot = ((plugin***)srv->plugin_slots)[i];

		if (slot) free(slot);
	}

	free(srv->plugin_slots);
	srv->plugin_slots = NULL;
	free(srv->plugins.ptr);
	srv->plugins.ptr = NULL;
	srv->plugins.used = 0;
}

int plugins_call_init(server* srv){

}

int plugins_call_cleanup(server* srv){

}

handler_t plugins_call_set_defaults(server* srv){

}
