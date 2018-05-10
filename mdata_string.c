#include "marray.h"
#include "mbase.h"
#include <stdio.h>

static struct data_unset* data_string_copy(const struct data_unset* s){
	data_string* src = (data_string*)s;
	data_string* ds = data_string_init();

	buffer_copy_buffer(ds->key, src->key);
	buffer_copy_buffer(ds->value, src->value);
	ds->is_index_key = src->is_index_key;

	return (data_unset*)ds;
}


static void data_string_free(struct data_unset* p){
	data_string* ds = (data_string*)p;

	buffer_free(ds->key);
	buffer_free(ds->value);
	free(p);
}


static void data_string_reset(struct data_unset* p){
	data_string* ds = (data_string*)p;

	buffer_reset(ds->key);
	buffer_reset(ds->value);
}


static int data_string_insert_dup(struct data_unset* d, struct data_unset* s){
	data_string* src = (data_string*)s;
	data_string* dst = (data_string*)d;

	if (!buffer_string_is_empty(dst->value)){
		buffer_append_string_len(dst->value, CONST_STR_LEN("\r\n"));
		buffer_append_string_buffer(dst->value, src->key);
		buffer_append_string_len(dst->value, CONST_STR_LEN(": "));
		buffer_append_string_buffer(dst->value, src->value);
	}else{
		buffer_copy_buffer(dst->value, src->value);
	}

	s->free(s);
	return 0;
}


static void data_string_print(struct data_unset* p, int depth){
	data_string* ds = (data_string*)p;
	size_t i, len;

	UNUSED(depth);

	if (buffer_string_is_empty(ds->value)){
		fprintf(stdout, "\"\"");
		return;
	}
	
	putc('"', stdout);
	len = buffer_string_length(ds->value);
	for (i = 0; i < len; i++){
		char c = ds->value->ptr[i];
		if (c == '"'){
			fprintf(stdout, "\\\"");
		}else{
			putc(c,stdout);
		}
	}
	putc('"', stdout);
}


data_string* data_string_init(){
	data_string* ds = calloc(1, sizeof(*ds));
	force_assert(ds != NULL);

	ds->key = buffer_init();
	ds->value = buffer_init();


	ds->copy = data_string_copy;
	ds->free = data_string_free;
	ds->reset = data_string_reset;
	ds->insert_dup = data_string_insert_dup;
	ds->print = data_string_print;
	return ds;
}