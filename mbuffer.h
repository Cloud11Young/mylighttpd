#ifndef MBUFFER_H_
#define MBUFFER_H_

#include <string.h>

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

int buffer_string_is_empty(buffer* b);
void buffer_copy_int(buffer* b, int d);
void buffer_append_string_len(buffer* b, const char* s, size_t s_len);

int buffer_string_length(buffer* b);

#define CONST_STR_LEN(x) x, (x) ? sizeof(x) - 1 : 0
#define CONST_BUF_LEN(x) ((x) ? (x)->ptr : NULL), buffer_string_length(x)

void log_failed_assert(const char* filename, unsigned int line, const char* s);

#define force_assert(x) do{ if(!(x))  log_failed_assert(__FILE__,__LINE__,"assertion failed: " #x); }while(0)

#endif