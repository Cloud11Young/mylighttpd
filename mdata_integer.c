#include "marray.h"
#include "mbase.h"
#include <stdio.h>

static struct data_unset* data_integer_copy(const struct data_unset* s){
	data_integer* src = (data_integer*)s;
	data_integer* di = data_integer_init();

	buffer_copy_buffer(di->key, src->key);
	di->is_index_key = src->is_index_key;
	di->value = src->value;

	return (data_unset*)di;
}


static void data_integer_free(struct data_unset* p){
	buffer_free(p->key);

	free(p);
}


static void data_integer_reset(struct data_unset* p){
	data_integer* di = (data_integer*)p;

	buffer_reset(p->key);
	di->value = 0;
}


static int data_integer_insert_dup(struct data_unset* det, struct data_unset* src){
	UNUSED(det);

	src->free(src);
	return 0;
}


static void data_integer_print(struct data_unset* p, int depth){
	data_integer* di = (data_integer*)p;
	UNUSED(depth);
	fprintf(stdout, "%d", di->value);
}


data_integer* data_integer_init(){
	data_integer* di = calloc(1, sizeof(*di));
	force_assert(di != NULL);

	di->key = buffer_init();

	di->copy = data_integer_copy;
	di->free = data_integer_free;
	di->reset = data_integer_reset;
	di->insert_dup = data_integer_insert_dup;
	di->print = data_integer_print;
	return di;
}