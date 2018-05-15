#include "marray.h"
#include "mbase.h"
#include <stdio.h>
#include <stdlib.h>


#define ARRAY_NOT_FOUND ((size_t)-1)

static size_t array_get_index(array* a, const char* key, size_t keylen, size_t* rndx){
	size_t lower = 0, upper = a->used;
	force_assert(upper < SSIZE_MAX);

	while (lower < upper){
		size_t probe = (lower + upper) / 2;
		force_assert(lower < upper);
		force_assert(lower <= probe && probe < upper);
		int comp = buffer_caseless_compare(key, keylen, CONST_BUF_LEN(a->data[a->sorted[probe]]->key));

		if (comp == 0){
			return a->sorted[probe];
		}else if (comp < 0){
			upper = probe;
		}else{
			lower = probe + 1;
		}
	}

	if (rndx)	*rndx = lower;
	return ARRAY_NOT_FOUND;
}


static data_unset** array_find_or_insert(array* a, data_unset* entry){
	size_t pos, ndx, j;

	if (buffer_is_empty(entry->key) || entry->is_index_key){
		buffer_copy_int(entry->key, a->unique_ndx++);
		entry->is_index_key = 1;
		force_assert(a->unique_ndx != 0);
	}

	if (ARRAY_NOT_FOUND != (ndx = array_get_index(a, CONST_BUF_LEN(entry->key), &pos))){
		return &a->data[ndx];
	}

	force_assert(a->used + 1 <= SSIZE_MAX);
	if (a->size == 0){
		a->size = 16;
		a->data = calloc(1, sizeof(*a->data) * a->size);
		a->sorted = calloc(1, sizeof(*a->sorted) * a->size);
		force_assert(a->data != NULL);
		force_assert(a->sorted != NULL);
		for (j = 0; j < a->size; j++)	a->data[j] = NULL;
	}else if (a->size == a->used){
		a->size += 16;
		a->data = realloc(a->data, sizeof(*a->data) * a->size);
		a->sorted = realloc(a->sorted, sizeof(*a->sorted) * a->size);
		force_assert(a->data != NULL);
		force_assert(a->sorted != NULL);
		for (j = a->used; j < a->size; j++)	a->data[j] = NULL;
	}

	ndx = a->used;
	
	if (a->data[ndx])	a->data[ndx]->free(a->data[ndx]);

	a->data[a->used++] = entry;

	if (ndx != pos){
		memmove(a->sorted + pos + 1, a->sorted + pos, (ndx - pos)*sizeof(*a->sorted));
	}
	a->sorted[pos] = ndx;
	return NULL;
}


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
	force_assert(a != NULL);
	data_unset** old;

	if (NULL != (old = array_find_or_insert(a, entry))){
		force_assert((*old)->type == entry->type);
		entry->insert_dup(*old, entry);
	}
}

data_unset* array_pop(array* a){
	data_unset* du;
	size_t i;
	force_assert(a->used != 0);
	
	a->used--;
	du = a->data[a->used];
//	force_assert(a->sorted[a->used] == a->used);
	for (i = 0; i <= a->used; i++){
		if (a->sorted[i] == a->used)
			break;
	}
	if (i < a->used){
		memmove(a->sorted + i, a->sorted + i + 1, a->used - i);
	}
	a->data[a->used] = NULL;
	return du;
}

void array_print(array* a, int depth){

}

data_unset* array_get_unused_element(array* a, data_type_t t){
	data_unset* du;
	size_t i;

	for (i = a->used; i < a->size; i++){
		if (a->data[i] && a->data[i]->type == t){
			du = a->data[i];

			a->data[i] = a->data[a->used];
			a->data[a->used] = NULL;

			return du;
		}
	}
	return NULL;
}

data_unset* array_get_element(array* a, const char* key){
	size_t ndx;
	force_assert(key != NULL);

	if (ARRAY_NOT_FOUND != (ndx = array_get_index(a, key, strlen(key), NULL))){
		return a->data[ndx];
	}

	return NULL;
}

data_unset* array_extract_element(array* a, const char* key){	/*removes found entry from array*/
	size_t ndx,pos;	
	force_assert(key != NULL);

	if (ARRAY_NOT_FOUND != (ndx = array_get_index(a, key, strlen(key), &pos))){
		size_t last_ndx = a->used - 1;
		data_unset* entry = a->data[ndx];

		if (last_ndx != ndx){
			size_t last_elem_pos;
			force_assert(last_ndx == array_get_index(a, CONST_BUF_LEN(a->data[last_ndx]->key), &last_elem_pos));
			
			a->data[ndx] = a->data[last_ndx];
			a->data[last_ndx] = NULL;
			a->sorted[last_elem_pos] = ndx;
		}else{
			a->data[ndx] = NULL;
		}
		
		if (pos != last_ndx){
			memmove(a->sorted + pos, a->sorted + pos + 1, (last_ndx - pos)*sizeof(*a->sorted));
		}
		a->sorted[last_ndx] = ARRAY_NOT_FOUND;
		a->used--;
		return entry;
	}

	return NULL;
}

void array_set_key_value(array* a, const char* key, size_t key_len, const char* value, size_t value_len){
	data_string* dst;

	if (NULL != (dst = (data_string*)array_get_element(a, key))){
		buffer_copy_string_len(dst->value, value, value_len);
		return;
	}

	if (NULL == (dst = (data_string*)array_get_unused_element(a, TYPE_STRING))){
		dst = data_string_init();
	}
	buffer_copy_string_len(dst->key, key, key_len);
	buffer_copy_string_len(dst->value, value, value_len);
	array_insert_unique(a, (data_unset*)dst);
}

void array_replace(array* a, data_unset* entry){
	data_unset** old;
	force_assert(entry != NULL);

	if (NULL != (old = array_find_or_insert(a, entry))){
		force_assert(*old != entry);
		(*old)->free(*old);
		*old = entry;
	}	
}

// int array_strcasecmp(const char* a, size_t a_len, const char* b, size_t b_len){
// 
// }

void array_print_indent(int depth){
	size_t i;
	for (i = 0; i < depth; i++){
		fprintf(stdout, "	");
	}
}

size_t array_get_max_key_length(array* a){
	size_t i;
	size_t maxlen = 0;

	for (i = 0; i < a->used; i++){
		data_unset* du = a->data[i];
		size_t len = buffer_string_length(du->key);
		if (len > maxlen)
			maxlen = len;
	}
	return maxlen;
}

