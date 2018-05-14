#include "marray.h"
#include "mbase.h"


static struct data_unset* data_array_copy(const struct data_unset* src){
	size_t i;
	data_array* da = (data_array*)src;
	data_array* dst = data_array_init();
	
// 	for (i = 0; i < da->value->used; i++){
// 		data_unset* du = da->value->data[i];
// 		array_insert_unique(dst->value, du->copy(du));
// 	}
	buffer_copy_buffer(dst->key, da->key);
	array_free(dst->value);
	dst->value = array_init_array(da->value);
	dst->is_index_key = da->is_index_key;

	return (data_unset*)dst;
}


static void data_array_free(struct data_unset* p){
	data_array* da = (data_array*)p;

	buffer_free(da->key);
	array_free(da->value);
	free(da);
	da = NULL;
}


static void data_array_reset(struct data_unset* p){
	data_array* da = (data_array*)p;

	buffer_reset(da->key);
	array_reset(da->value);
}


static int data_array_insert_dup(struct data_unset* det, struct data_unset* src){
	UNUSED(det);
	src->free(src);
	return 0;
}


static void data_array_print(struct data_unset* p, int depth){
	data_array* da = (data_array*)p;
	array_print(da->value, depth);
}


data_array* data_array_init(){
	data_array* da = malloc(sizeof(*da));
	force_assert(da != NULL);

	da->type = TYPE_ARRAY;
	da->copy = data_array_copy;
	da->free = data_array_free;
	da->reset = data_array_reset;
	da->insert_dup = data_array_insert_dup;
	da->print = data_array_print;

	da->value = array_init();
	return da;
}