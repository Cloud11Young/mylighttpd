#include "mconfigfile.h"

#include "mbase.h"

static void config_init(server* srv,config_t* config){
	force_assert(config != NULL);
	config->srv = srv;
	config->ok = 1;
	vector_config_weak_init(&config->configs_stack);
	config->basedir = buffer_init();
}


static void config_free(config_t* config){
	vector_config_weak_free(&config->configs_stack);
	buffer_free(config->basedir);
}


int config_read(server* srv, const char* fn){
	config_t context;
	data_config* dc;
	data_integer* dpid;
	data_string* dcwd;
	buffer* filename;
	char* pos;
	int ret;

	config_init(srv, &context);
	context.all_configs = srv->config_context;

	pos = strrchr(fn, '/');
	if (pos)
		buffer_copy_string_len(context.basedir, fn, pos - fn + 1);

	dc = data_config_init();
	buffer_copy_string_len(dc->key, CONST_STR_LEN("global"));

	force_assert(context.all_configs->used == 0);
	dc->context_ndx = context.all_configs->used;
	array_insert_unique(context.all_configs, (data_unset*)dc);
	context.current = dc;

	dpid = data_integer_init();
	dpid->value = getpid();
	buffer_copy_string_len(dpid->key, CONST_STR_LEN("var.PID"));
	array_insert_unique(dc->value, (data_unset*)dpid);

	dcwd = data_string_init();
	buffer_string_prepare_copy(dcwd->value, 1023);
	if (NULL != getcwd(dcwd->value->ptr, dcwd->value->size - 1)){
		buffer_commit(dcwd->value, strlen(dcwd->value->ptr));
		buffer_copy_string_len(dcwd->key, CONST_STR_LEN("var.CWD"));
		array_insert_unique(dc->value, (data_unset*)dcwd);
	}else{
		dcwd->free((data_unset*)dcwd);
	}

	filename = buffer_init_string(fn);
	ret = config_parse_file_stream(srv, &context, filename);
	if (ret != 0)
		return ret;
	if (0 != config_insert(srv))
		return -1;
	return 0;
}

int config_set_defaults(server* srv){

}