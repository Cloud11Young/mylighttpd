#include "mbuffer.h"
#include "msettings.h"
#include <stdlib.h>

static char hex_chars[] = "0123456789abcdef";

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
	if (b == NULL)	return;

	if (b->size > BUFFER_MAX_REUSE_SIZE){
		free(b->ptr);
		b->ptr = NULL;
		b->size = 0;
	}else if (b->size > 0){
		b->ptr[0] = '\0';
	}
	b->used = 0;
}

void buffer_copy_string(buffer* b, const char* s){
// 	if (b == NULL){
// 		b = buffer_init_string(s);
// 		return;
// 	}
// 	if (b->size < strlen(s) + 1){
// 		b->ptr = realloc(b, strlen(s) + 1);
// 		b->size = strlen(s) + 1;
// 	}
// 	memcpy(b->ptr, s, b->size);
// 	b->used = b->size;	
	buffer_copy_string_len(b, s, (s == NULL) ? 0 : strlen(s));
}

void buffer_copy_string_len(buffer* b, const char* s, size_t s_len){
	force_assert(NULL != b);
	force_assert(NULL != s && s_len != 0);
	buffer_string_prepare_copy(b, s_len);
	if (0 != s_len) memcpy(b->ptr, s, s_len);
	buffer_commit(b, s_len);
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

	if(b->used == 0)	b->used = 1;

	if (size > 0){
		force_assert(b->used + size > b->used);
		force_assert(b->used + size >= b->size);
		b->used += size;
	}

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

void buffer_append_string(buffer* b, const char* s){
	int nlen = 0;
	if (s != NULL)
		nlen = strlen(s);
	return buffer_append_string_len(b, s, nlen);
}


void buffer_append_strftime(buffer* b, const char* format, const struct tm* tm){
	force_assert(NULL != b);
	force_assert(NULL != tm);

	char* buf;
	size_t r;
	buf = buffer_string_prepare_append(b, 255);
	r = strftime(buf, buffer_string_space(b), format, tm);
	if (r == 0 || r > buffer_string_space(b)){
		buf = buffer_string_prepare_append(b, 4095);
		r = strftime(buf, buffer_string_space(b), format, tm);
	}

	if (r > buffer_string_space(b))	r = 0;
	buffer_commit(b, r);
}

void buffer_append_string_c_escaped(buffer* b, const char* s, size_t s_len){
	unsigned char *ds, *buf;
	size_t d_len,ndx;

	force_assert(NULL != b);
	force_assert(NULL != s || 0 == s_len);

	if (s_len == 0)	return;
	for (ds = (unsigned char*)s, d_len = 0, ndx = 0; ndx < s_len; ds++, ndx++){
		if (*ds < 0x20 || *ds > 0x7f){
			switch (*ds){
			case '\t':
			case '\n':
			case '\r':
				d_len += 2;
				break;
			default:
				d_len += 4;
				break;
			}
		}else{
			d_len++;
		}
	}
	buf = buffer_string_prepare_append(b, d_len);
	buffer_commit(b, d_len);
	force_assert(buf[0] == '\0');

	for (ds=(unsigned char*)s, d_len = 0, ndx = 0; ndx < s_len; ds++, ndx++){
		if (*ds < 0x20 || *ds >0x7f){
			buf[d_len++] = '\\';
			switch (*ds){
			case '\t':
				buf[d_len++] = 't';
				break;
			case '\n':
				buf[d_len++] = 'n';
				break;
			case '\r':
				buf[d_len++] = 'r';
				break;
			default:
				buf[d_len++] = 'x';
				buf[d_len++] = hex_chars[(*ds) >> 4 & 0x0F];
				buf[d_len++] = hex_chars[(*ds) & 0x0F];
				break;
			}
		}else{
			buf[d_len++] = *ds;
		}
	}
}

void buffer_append_uint_hex(buffer* b, uintmax_t value){
	force_assert(NULL != b);
	char* buf;
	int shift = 0;
	uintmax_t copy = value;
	do{
		copy >>= 8;
		shift += 2;
	} while (copy > 0);

	buf = buffer_string_prepare_append(b, shift);
	buffer_commit(b, shift);	
	
	shift <<= 2;
	while (shift > 0){
		shift -= 4;
		*(buf++) = hex_chars[(value >> shift) & 0x0F];	
	}
}

int buffer_string_space(buffer* b){
	if (NULL == b || 0 == b->size)	return 0;
	return b->size - b->used - 1;
}


void buffer_move(buffer* b, buffer* src){
	buffer tmp;
	if (b == NULL){
		buffer_reset(src);
		return;
	}

	buffer_reset(b);
	if (src == NULL)	return;

	tmp = *src; *src = *b; *b = tmp;
}


int buffer_caseless_compare(const char* a, size_t a_len, const char* b, size_t b_len){
	const size_t len = a_len < b_len ? a_len : b_len;
	size_t i;

	for (i = 0; i < len; i++){
		unsigned char ca = a[i], cb = b[i];
		if (ca == cb)	continue;

		if (ca >= 'A' && ca <= 'Z')	ca |= 32;
		if (cb >= 'A'&& cb <= 'Z')	cb |= 32;

		if (ca == cb)	continue;
		return (int)ca - (int)cb;
	}

	if (a_len == b_len)	return 0;
	return a_len < b_len ? -1 : 1;
}


void buffer_to_lower(buffer* b){
	size_t i;

	for (i = 0; i < b->used; i++){
		char ch = b->ptr[i];
		if (ch >= 'A'&& ch <= 'Z')	ch |= 0x20;
	}
}


void buffer_to_upper(buffer* b){
	size_t i;
	for (i = 0; i < b->used; i++){
		char ch = b->ptr[i];
		if (ch >= 'a' && ch <= 'z')	ch &= ~0x20;
	}
}


void buffer_append_string_buffer(buffer* b, const buffer* src){
	if (NULL == src){
		buffer_append_string_len(b, NULL, 0);
	}else{
		buffer_append_string_len(b, src->ptr, buffer_string_length(src));
	}
}


void buffer_string_set_length(buffer* b, size_t len){
	force_assert(b != NULL);
	force_assert(len + 1 > len);

	buffer_realloc(b, len + 1);

	b->used = len + 1;
	b->ptr[len] = '\0';
}