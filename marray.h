#ifndef MARRAY_H_
#define MARRAY_H_

typedef enum{ TYPE_UNSET, TYPE_STRING, TYPE_OTHER, TYPE_ARRAY, TYPE_INTEGER, TYPE_FASTCGI, TYPE_CONFIG }data_type_t;
#define DATA_UNSET\
	data_type_t type; \
	buffer* key; \
	int is_index_key; \
	struct data_unset* (*copy)(const struct data_unset* src); \
	void (*free)(struct data_unset* p); \
	void (*reset)(struct data_unset* p); \
	int (*insert_dup)(struct data_unset* det, struct data_unset* src); \
	void (*print)(struct data_unset* p, int depth);\

typedef struct {
	DATA_UNSET;
}data_unset;

typedef struct{
	DATA_UNSET;
	buffer* value;
}data_string;

data_string* data_string_init();
data_string* data_response_init();


typedef struct{
	data_unset** data;
	size_t* sorted;
	size_t used;
	size_t size;
	size_t unique_ndx;
}array;

typedef struct{
	DATA_UNSET;
	array* value;
}data_array;

data_array* data_array_init();

typedef struct{
	DATA_UNSET;

	array* value;
	buffer* comp_key;
	comp_key_t comp;

	config_cond_t cond;
	buffer* op;

	int context_ndx;
	vector_config_weak children;

	data_config* parent;
	data_config* prev;
	data_config* next;

	buffer* string;
#ifdef HAVE_PCRE_H
	pcre* regex;
	pcre_extra* regex_study;
#endif
}data_config;

data_config* data_config_init();

typedef struct {
	DATA_UNSET;
	int value;
}data_integer;

data_integer* data_integer_init();

typedef struct {
	DATA_UNSET;

	buffer* host;
	unsigned short port;
	time_t disable_ts;
	int is_disabled;
	size_t balance;

	int usage;
	int last_used_ndx;
}data_fastcgi;

data_fastcgi* data_fastcgi_init();

array* array_init();
array* array_init_array(array* src);
void array_free(array* a);
void array_reset(array* a);
void array_insert_unique(array* a, data_unset* entry);
data_unset* array_pop(array* a);
void array_print(array* a, int depth);
data_unset* array_get_unused_element(array* a, data_type_t t);
data_unset* array_get_element(array* a, const char* key);
data_unset* array_extract_element(array* a, const char* key);	/*removes found entry from array*/
void array_set_key_value(array* a, const char* key, size_t key_len, const char* value, size_t value_len);
void array_replace(array* a, data_unset* entry);
int array_strcasecmp(const char* a, size_t a_len, const char* b, size_t b_len);
void array_print_indent(int depth);
size_t array_get_max_key_length(array* a);

#endif