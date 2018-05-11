#include "mconfigfile.h"

int config_insert_values_global(server* srv, array* a, const config_values_t* cv, config_scope_type_t cst){
	size_t i;
	data_unset* du;

	for (i = 0; cv[i].key; i++){
		data_string* touched;

		if (NULL == (du = array_get_element(a, cv[i].key))){
			continue;
		}
		touched = data_string_init();
		buffer_copy_string_len(touched->value, CONST_STR_LEN(""));
		buffer_copy_buffer(touched->key, du->key);
		array_insert_unique(srv->config_touched, (data_unset*)touched);
	}

	return config_insert_values_internal(srv, a, cv, cst);
}


int config_insert_values_internal(server* srv, array* a, const config_values_t* cv, config_scope_type_t scope){
	size_t i;
	data_unset* du;

	for (i = 0; cv[i].key; i++){
		if (NULL == (du = array_get_element(a, cv[i].key))){
			continue;
		}

		if (T_CONFIG_SCOPE_SERVER == cv[i].scope &&
			T_CONFIG_SCOPE_SERVER != scope){
			log_error_write(srv, __FILE__, __LINE__, "ssss",
				"DEPRECATED: don't set server options in conditionals, variable", cv[i].key);
		}

		switch (cv[i].type){
		case T_CONFIG_STRING:
			if (du->type == TYPE_STRING){
				data_string* ds = (data_string*)ds;
				buffer_copy_buffer(cv[i].destination, ds->value);
			}else{
				log_error_write(srv, __FILE__, __LINE__, "ss", cv[i].key, " should have been a string like ... = \"...\" ");
				return -1;
			}
			break;
		case T_CONFIG_INT:
			switch (du->type){
			case TYPE_INTEGER:{
				data_integer* di = (data_integer*)du;
				*((unsigned int*)cv[i].destination) = di->value;
				break;
			}
			case TYPE_STRING:{				
				data_string* ds = (data_string*)du;
				if (ds->value->ptr && *ds->value->ptr){
					 char* e;
					 long l = strtol(ds->value->ptr, &e, 10);
					 if (e != ds->value->ptr && !*e && l >= 0){
						 *((unsigned int*)cv[i].destination) = l;
						 break;
					 }
				}
				log_error_write(srv, __FILE__, __LINE__, "ssb", "got a string but expected an integer", cv[i].key, ds->value);
				return -1;
			}
			default:
				log_error_write(srv, __FILE__, __LINE__, "ssds", "unexpected type for key: ", cv[i].key, du->type, "expected an integer , rang 0 ...");
				return -1;
			}
			break;
		case T_CONFIG_SHORT:
			switch (du->type){
			case TYPE_INTEGER:{
				data_integer* di = (data_integer*)du;
				*((unsigned short*)cv[i].destination) = di->value;
				break;
			}
			case TYPE_STRING:{
				data_string* ds = (data_string*)du;
				if (ds->value->ptr && *ds->value->ptr){
					 char* e;
					 long l = strtol(ds->value->ptr, &e, 10);
					 if (e != ds->value->ptr && !*e && l >= 0 && l < 65535){
						 *((unsigned short*)cv[i].destination) = l;
						 break;
					 }
				}
				log_error_write(srv, __FILE__, __LINE__, "ssb", "got a string but expected an short", cv[i].key, ds->value);
				return -1;
			}
			default:
				log_error_write(srv, __FILE__, __LINE__, "ssds", "unexpected type for key: ", cv[i].key, du->type, "expected an short , rang 0 ...");
				return -1;
			}
			break;
		case T_CONFIG_ARRAY:
			if (du->type = TYPE_ARRAY){
				size_t j;
				data_array* da = (data_array*)du;

				for (j = 0; j < da->value->used; j++){
					data_unset* ds = da->value->data[j];
					if (ds->type == TYPE_STRING){
						array_insert_unique(cv[i].destination, ds->copy(ds));
					}else {
						log_error_write(srv, __FILE__, __LINE__, "sssssd",
							"the value of an array can only be a string, variable: ",
							cv[i].key, "[", ds->key, "], type", ds->type);
						return -1;
					}
				}
			}else{
				log_error_write(srv, __FILE__, __LINE__, "ss",
					cv[i].key, "should have been an array of string like ... = (\"...\")");
				return -1;
			}
			break;
		case T_CONFIG_BOOLEAN:
			if (du->type == TYPE_STRING){
				data_string* ds = (data_string*)du;

				if (buffer_is_equal_string(ds->value, CONST_STR_LEN("enable"))){
					*((unsigned short*)cv[i].destination) = 1;
				}else if (buffer_is_equal_string(ds->value,CONST_STR_LEN("disable"))){
					*((unsigned short*)cv[i].destination) = 0;
				}else{
					log_error_write(srv, __FILE__, __LINE__, "ssbs", "ERROR: unexpected value for key: ", cv[i].key, ds->value, "(enable | disable)");
					return -1;
				}
			}else{
				log_error_write(srv, __FILE__, __LINE__, "ssss", "ERROR: unexpected type for key ", cv[i].key, "string", "\"(enable|disable)\"");
				return -1;
			}
			break;
		case T_CONFIG_UNSET:
		case T_CONFIG_LOCAL:
			break;
		case T_CONFIG_UNSUPPORTED:
			log_error_write(srv, __FILE__, __LINE__, "ssss", "ERROR: found unsupported key:", cv[i].key, "-", (char*)cv[i].destination);
			srv->config_unsupported = 1;
			break;
		case T_CONFIG_DEPRECATED:
			log_error_write(srv, __FILE__, __LINE__, "ssss", "ERROR: found deprecated key:", cv[i].key, "-", (char*)cv[i].destination);
			srv->config_deprecated = 1;
			break;
		}
	}

	return 0;
}