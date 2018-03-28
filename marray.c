#include "marray.h"
#include <stdlib.h>

array* array_init(){
	array* a;
	a = calloc(1, sizeof(*a));
	force_assert(a);
	return a;
}

array* array_init_array(array* src){
	array* a = array_init();
	size_t i;

	if (src->size == 0)	return a;

	a->size = src->size;
	a->used = src->used;
	a->unique_ndx = src->unique_ndx;

	a->data = malloc(sizeof(*src->data)*src->size);
	force_assert(a->data);
	for (i = 0; i < src->size; i++){
		if (src->data[i]) a->data[i] = src->data[i]->copy(src->data[i]);
		else a->data[i] = NULL;
	}
	a->sorted = malloc(sizeof(*src->sorted)*src->size);
	force_assert(a->sorted);
	memcpy(a->sorted, src->sorted, sizeof(*src->sorted)*src->size);
	return a;
}

void array_free(array* a){
	size_t i;
	if (!a)	return;
	
	for (i = 0; i < a->size; i++){
		if(a->data[i])	a->data[i]->free(a->data[i]);
	}

	if (a->data)	free(a->data);
	if (a->sorted)	free(a->sorted);
	free(a);
}

void array_reset(array* a){
	size_t i;
	if (!a)	return;
	for (i = 0; i < a->used; i++){
		if (a->data[i]) a->data[i]->reset(a->data[i]);
	}
	a->used = 0;
	a->unique_ndx = 0;
}

void array_insert_unique(array* a, data_unset* entry){

}

data_unset* array_pop(array* a){

}

void array_print(array* a, int depth){

}

data_unset* array_get_unused_element(array* a, data_type_t t){

}

data_unset* array_get_element(array* a, const char* key){

}

data_unset* array_extract_element(array* a, const char* key){	/*removes found entry from array*/

}

void array_set_key_value(array* a, const char* key, size_t key_len, const char* value, size_t value_len){

}

void array_replace(array* a, data_unset* entry){

}

int array_strcasecmp(const char* a, size_t a_len, const char* b, size_t b_len){

}

void array_print_indent(int depth){

}

size_t array_get_max_key_length(array* a){

}

