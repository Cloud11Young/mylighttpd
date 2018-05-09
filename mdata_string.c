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


static int data_string_insert_dup(struct data_unset* dst, struct data_unset* s){
	UNUSED(det);

	src->free(src);
	return 0;
}


static void data_string_print(struct data_unset* p, int depth){
	data_string* di = (data_string*)p;
	UNUSED(depth);
	fprintf(stdout, "%s", di->value->ptr);
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