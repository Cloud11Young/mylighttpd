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

int buffer_is_empty(const buffer* b){
	return b->ptr == NULL || b->used == 0;
}

int buffer_string_is_empty(const buffer* b){
	return 0 == buffer_string_length(b);
}

void log_failed_assert(const char* filename, unsigned int line, const char* s){

}

#define BUFFER_PIECE_SIZE 64
size_t buffer_align_size(size_t size){
	size_t align = BUFFER_PIECE_SIZE - size % BUFFER_PIECE_SIZE;
	if (align + size < size) return size;
	return align + size;
}

static void buffer_alloc(buffer* b, size_t size){
	force_assert(b != NULL);
	if (size == 0) size = 1;

	if (size <= b->size) return;

	if (NULL != b->ptr)	free(b->ptr);

	b->used = 0;
	b->size = buffer_align_size(size);
	b->ptr = malloc(b->size);
	force_assert(NULL != b->ptr);
}

static void buffer_realloc(buffer* b, size_t size){
	force_assert(NULL != b);
	if (size == 0) size = 1;

	if (size <= b->size) return;

	b->size = buffer_align_size(size);
	b->ptr = realloc(b->ptr, b->size);
	force_assert(NULL != b->ptr);
}

char* buffer_string_prepare_copy(buffer* b,size_t size){
	force_assert(b != NULL);
	force_assert(size + 1 > size);

	buffer_alloc(b, size + 1);
	b->used = 0;
	b->ptr[0] = 0;
	return b->ptr;
}

void buffer_copy_int(buffer* b, intmax_t d){
	force_assert(b);
	b->used = 0;
	buffer_append_int(b, d);
}

static char* utostr(char* buf, uintmax_t val){
	char* str = buf;
	do{
		uintmax_t mod = val % 10;
		val /= 10;
		*(--str) = (char)('0' + mod);
	} while (val);
	return str;
}

static char* itostr(char* buf, intmax_t val){
	uintmax_t uval = val > 0 ? (uintmax_t)val : ((uintmax_t)~val) + 1;
	char* str = utostr(buf, uval);
	return str;
}

void buffer_append_int(buffer* b, intmax_t val){
	char buf[LI_ITOSTRING_LENGTH] = { 0 };
	char* const buf_end = buf + sizeof(buf);
	char* str;
	
	str = itostr(buf_end, val);

	buffer_append_string_len(b, str, buf_end - str);
}

char* buffer_string_prepare_append(buffer* b, size_t size){
	force_assert(b != NULL);

	if (buffer_string_is_empty(b)){
		return buffer_string_prepare_copy(b, size);
	}else{
		size_t req_size = b->used + size;
		force_assert(req_size >= b->used);
		buffer_realloc(b, req_size);
		return b->ptr + b->used - 1;
	}
}

void buffer_append_string_len(buffer* b, const char* s, size_t s_len){
	char* target_buf;
	force_assert(b != NULL);
	force_assert(s != NULL || s_len == 0);

	target_buf = buffer_string_prepare_append(b, s_len);
	if (s_len == 0)	return;

	memcpy(target_buf, s, s_len);
	buffer_commit(b, s_len);
}

void buffer_commit(buffer* b, size_t size){
	force_assert(b != NULL);
	force_assert(b->size > 0);
	if (size > 0){
		force_assert(b->used + size > b->used);
		force_assert(b->used + size >= b->size);
	}
	b->used += size;
	b->ptr[b->used - 1] = '\0';
}

int buffer_string_length(const buffer* b){
	return b != NULL && b->used != 0 ? b->used - 1 : 0;
}

int buffer_is_equal(const buffer* a, const buffer* b){
	force_assert(a != NULL && b != NULL);
	
	if (a->used != b->used)	return 0;
	if (a->used == 0)	return 1;

	return (0 == memcmp(a->ptr, b->ptr, a->used));
}

int buffer_is_equal_string(const buffer* a, const char* s, size_t b_len){
	force_assert(NULL != a && NULL != s);
	force_assert(b_len + 1 > b_len);

	if (a->used != b_len + 1)	return 0;
	if (0 != memcmp(a->ptr, s, b_len))	return 0;
	if ('\0' != a->ptr[a->used - 1])	return 0;

	return 1;
}
