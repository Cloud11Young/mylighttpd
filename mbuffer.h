#ifndef MBUFFER_H_
#define MBUFFER_H_

#include <string.h>
#include <stdint.h>
#include <time.h>

typedef struct buffer{
	char* ptr;
	size_t size;
	size_t used;
}buffer;

buffer* buffer_init();
buffer* buffer_init_buffer(const buffer* b);
buffer* buffer_init_string(const char* s);

void buffer_free(buffer* b);
void buffer_reset(buffer* b);

void buffer_copy_string(buffer* b, const char* s);
void buffer_copy_buffer(buffer* b, const buffer* src);

void buffer_copy_string_len(buffer* b, const char* s, size_t s_len);

int buffer_is_empty(const buffer* b);
int buffer_string_is_empty(const buffer* b);
char* buffer_string_prepare_copy(buffer* b, size_t size);
void buffer_copy_int(buffer* b, intmax_t d);
void buffer_append_int(buffer* b, intmax_t val);
void buffer_append_string_len(buffer* b, const char* s, size_t s_len);
char* buffer_string_prepare_append(buffer* b, size_t size);

int buffer_is_equal(const buffer* a, const buffer* b);
int buffer_is_equal_string(const buffer* a, const char* s, size_t b_len);
void buffer_commit(buffer* b, size_t size);

void buffer_append_string(buffer* b, const char* s);
void buffer_append_strftime(buffer* b, const char* format, const struct tm* tm);
void buffer_append_string_c_escaped(buffer* b, const char* s, size_t s_len);
void buffer_append_uint_hex(buffer* b, uintmax_t len);
void buffer_append_string_buffer(buffer* b, const buffer* src);

void buffer_string_set_length(buffer* b, size_t len);

int buffer_string_space(buffer* b);

void buffer_move(buffer* b, buffer* src);

int buffer_caseless_compare(const char* a, size_t a_len, const char* b, size_t b_len);

#define LI_ITOSTRING_LENGTH (2 + (8 * sizeof(intmax_t) * 31 + 99) / 100)

int buffer_string_length(const buffer* b);

#define CONST_STR_LEN(x) x, (x) ? sizeof(x) - 1 : 0
#define CONST_BUF_LEN(x) ((x) ? (x)->ptr : NULL), buffer_string_length(x)
#define BUFFER_APPEND_STRING_CONST(x,y)\
	buffer_append_string_len(x,y,sizeof(y)-1)

void log_failed_assert(const char* filename, unsigned int line, const char* s);

#define force_assert(x) do{ if(!(x))  log_failed_assert(__FILE__,__LINE__,"assertion failed: " #x); }while(0)

static inline void buffer_append_slash(buffer* b){
	size_t len = buffer_string_length(b);
	if (len > 0 && b->ptr[len - 1] != '/')
		BUFFER_APPEND_STRING_CONST(b, "/");
}
#endif