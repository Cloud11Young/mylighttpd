#include "metag.h"

int etag_create(buffer* etag, struct stat* st, etag_flags_t flags){
	if (flags == 0)	return 0;
	
	buffer_reset(etag);

	if (flags & ETAG_USE_INODE){
		buffer_append_int(etag, st->st_ino);
		buffer_append_string_len(etag, CONST_STR_LEN("-"));
	}

	if (flags & ETAG_USE_SIZE){
		buffer_append_int(etag, st->st_size);
		buffer_append_string_len(etag, CONST_STR_LEN("-"));
	}

	if (flags & ETAG_USE_MTIME){
		buffer_append_int(etag, st->st_mtime);
	}

	return 0;
}


int etag_is_equal(buffer* etag, const char* matches, int weak_ok){

}


int etag_mutate(buffer* mut, buffer* etag){

}