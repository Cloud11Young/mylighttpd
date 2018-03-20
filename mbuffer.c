#include "mbuffer.h"
#include <stdlib.h>

buffer* buffer_init(){
	buffer* b = malloc(sizeof(*b));
	force_assert(b);

	b->ptr = NULL;
	b->size = 0;
	b->used = 0;
	return b;
}

buffer* buffer_init_buffer(const buffer* src){
	buffer* b = buffer_init();
	b->size = src->size;
	b->ptr = malloc(src->size);
	memcpy(b->ptr, src->ptr, src->size);
	b->used = src->used;
	return b;
}

buffer* buffer_init_string(const char* s){
	buffer* b = buffer_init();
	b->size = strlen(s) + 1;
	b->ptr = malloc(b->size);
	memcpy(b->ptr, s, b->size);
	b->used = b->size;
	return b;
}

void buffer_free(buffer* b){
	if (b != NULL){
		if (b->ptr != NULL){
			free(b->ptr);
			b->ptr = NULL;
		}
		free(b);
		b = NULL;
	}
}

void buffer_reset(buffer* b){
	force_assert(b);
	b->ptr[0] = '\0';
	b->used = 0;
}

void buffer_copy_string(buffer* b, const char* s){
	if (b == NULL){
		b = buffer_init_string(s);
		return;
	}
	if (b->size < strlen(s) + 1){
		b->ptr = realloc(b, strlen(s) + 1);
		b->size = strlen(s) + 1;
	}
	memcpy(b->ptr, s, b->size);
	b->used = b->size;	
}

void buffer_copy_buffer(buffer* b, const buffer* src){
	buffer_free(b);
	b = buffer_init_buffer(src);
}

int buffer_string_is_empty(buffer* b){
	return b->ptr == NULL || b->used == 0;
}

void log_failed_assert(const char* filename, unsigned int line, const char* s){

}

void buffer_copy_int(buffer* b, int d){

}

void buffer_append_string_len(buffer* b, const char* s, size_t s_len){

}

int buffer_string_length(buffer* b){

}
