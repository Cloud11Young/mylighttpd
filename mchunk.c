#include "mchunk.h"
#include "mbase.h"
#include <sys/mman.h>
#include <errno.h>

#define DEFAULT_TEMPFILE_SIZE (1 * 1024 * 1024)
#define MAX_TEMPFILE_SIZE (128 * 1024 * 1024)

static array* chunkqueue_default_tempdirs = NULL;
static unsigned int chunkqueue_default_tempdirs_size = DEFAULT_TEMPFILE_SIZE;


static chunk* chunk_init(){
	chunk* c = calloc(1, sizeof(*c));
	force_assert(c != NULL);
	
	c->type = MEM_CHUNK;
	c->mem = buffer_init();
	c->file.name = buffer_init();
	c->file.start = c->file.length = 0;
	c->file.fd = -1;
	c->file.mmap.start = MAP_FAILED;
	c->file.mmap.offset = 0;
	c->file.is_temp = 0;
	c->next = NULL;
}


static void chunk_reset(chunk* c){
	if (c == NULL)	return;
	c->type = MEM_CHUNK;
	buffer_reset(c->mem);
	
	if (c->file.is_temp && !buffer_string_is_empty(c->file.name))
		unlink(c->file.name->ptr);
	buffer_reset(c->file.name);
	c->file.is_temp = 0;
	
	if (MAP_FAILED != c->file.mmap.start){
		munmap(c->file.mmap.start, c->file.mmap.length);
		c->file.mmap.start = MAP_FAILED;
	}

	c->file.start = c->file.length = c->file.mmap.length = 0;
	c->file.mmap.offset = 0;
	c->file.fd = -1;
	c->next = NULL;
}


static void chunk_free(chunk* c){
	if (c == NULL)	return;

	chunk_reset(c);

	buffer_free(c->mem);
	buffer_free(c->file.name);

	free(c);
}


static chunk* chunkqueue_get_unused_chunk(chunkqueue* cq){
	chunk* c;

	force_assert(cq != NULL);

	if (cq->unused != NULL){
		c = cq->unused;
		cq->unused = c->next;
		c->next = NULL;
		cq->unused_chunks--;	
	}else{
		c = chunk_init();
	}

	return c;
}


static void chunkqueue_push_unused_chunk(chunkqueue* cq, chunk* c){
	chunk* pc;

	if (cq == NULL || c == NULL)	return;

	if (cq->unused_chunks > 4){
		chunk_free(c);
	}else{
		chunk_reset(c);
		c->next = cq->unused;
		cq->unused = c;
		cq->unused_chunks++;
	}
	
}


static off_t chunk_remaining_length(chunk* c){
	off_t len = 0;

	force_assert(c != NULL);

	switch (c->type){
	case MEM_CHUNK:
		len = buffer_string_length(c->mem);
		break;
	case FILE_CHUNK:
		len = c->file.length;
		break;
	default:
		force_assert(c->type == MEM_CHUNK || c->type == FILE_CHUNK);
		break;
	}

	force_assert(len >= c->offset);
	return len - c->offset;
}

static chunk* chunkqueue_get_append_tempfile(chunkqueue* cq){
	int fd;
	chunk* c;
	buffer* template = buffer_init_string("/var/tmp/lighttpd-upload-xxxxxx");

	if (cq->tempdirs && cq->tempdirs->used){
		for (errno = EIO; cq->tempdir_idx < cq->tempdirs->used; cq->tempdir_idx++){
			data_string* ds = (data_string*)cq->tempdirs->data[cq->tempdir_idx];
			buffer_copy_buffer(template, ds->value);
			buffer_append_slash(template);
			buffer_append_string_len(template, CONST_STR_LEN("lighttpd-upload-XXXXXX"));

			if (-1 != (fd = mkstemp(template->ptr)))	break;
		}
	}else{
		fd = mkstemp(template->ptr);
	}
	if (fd < 0){
		buffer_free(template);
		return NULL;
	}

	if (0 != fcntl(fd, F_SETFL, (fcntl(fd, F_GETFL, 0) | O_APPEND))){
		close(fd);
		buffer_free(template);
		return NULL;
	}

	fd_close_on_exec(fd);

	c = chunkqueue_get_unused_chunk(cq);
	c->type = FILE_CHUNK;
	c->file.fd = fd;
	c->file.is_temp = 1;
	buffer_copy_buffer(c->file.name, template);
	c->file.length = 0;
	chunkqueue_append_chunk(cq, c);
	buffer_free(template);
	return c;
}


static void chunkqueue_remove_empty_chunks(chunkqueue* cq){
	chunk* c;
	chunkqueue_remove_finished_chunks(cq);
	if (chunkqueue_is_empty(cq))	return;

	for (c = cq->first; c->next; c = c->next){
		if (0 == chunk_remaining_length(c->next)){
			chunk* empty = c->next;
			c->next = empty->next;
			if (empty == cq->last) cq->last = c;

			chunkqueue_push_unused_chunk(cq, empty);
		}
	}
}


chunkqueue* chunkqueue_init(){
	chunkqueue* cq = calloc(1, sizeof(*cq));
	force_assert(cq != NULL);
	
	cq->first = NULL;
	cq->last = NULL;
	cq->unused = NULL;
	cq->tempdirs = chunkqueue_default_tempdirs;
	cq->upload_temp_file_size = chunkqueue_default_tempdirs_size;

	return cq;
}

void chunkqueue_set_tempdirs_default(array* tempdirs, unsigned int upload_temp_file_size){
	chunkqueue_default_tempdirs = tempdirs;
	chunkqueue_default_tempdirs_size = upload_temp_file_size == 0 ? DEFAULT_TEMPFILE_SIZE :
		(upload_temp_file_size > MAX_TEMPFILE_SIZE ? MAX_TEMPFILE_SIZE : upload_temp_file_size);
}


void chunkqueue_append_file(chunkqueue* cq, buffer* fn, off_t offset, off_t len){
	if (cq == NULL)	return;
	if (len == 0)	return;
	
	chunk* c = chunkqueue_get_unused_chunk(cq);
	force_assert(c != NULL);

	c->type = FILE_CHUNK;
	buffer_copy_buffer(c->file.name, fn);
	
	c->offset = 0;
	c->file.start = offset;
	c->file.length = len;

	chunkqueue_append_chunk(cq, c);
}


void chunkqueue_append_file_fd(chunkqueue* cq, buffer* fn, int fd, off_t offset, off_t len){
	if (cq == NULL) return;
	if (len == 0){
		close(fd);
		return;
	}
	
	chunk* c = chunkqueue_get_unused_chunk(cq);
	force_assert(c != NULL);

	c->type = FILE_CHUNK;
	buffer_copy_buffer(c->file.name, fn);
	c->file.start = offset;
	c->file.length = len;
	c->offset = 0;
	c->file.fd = fd;

	chunkqueue_append_chunk(cq, c);
}


void chunkqueue_append_mem(chunkqueue* cq, const char* mem, size_t len){
	/*buffer* b = buffer_init();
	buffer_copy_string_len(b, mem, len);

	chunkqueue_append_buffer(cq, b);*/

	if (cq == NULL || len == 0)	return;
	chunk* c = chunkqueue_get_unused_chunk(cq);

	c->type = MEM_CHUNK;
	buffer_copy_string_len(c->mem, mem, len);

	chunkqueue_append_chunk(cq, c);
}


void chunkqueue_append_buffer(chunkqueue* cq, buffer* mem){
	if (cq == NULL || mem == NULL)	return;
	
	if (buffer_string_is_empty(mem))	return;

	chunk* c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;

	buffer_move(c->mem, mem);
	c->offset = 0;

	chunkqueue_append_chunk(cq, c);
}

void chunkqueue_prepend_buffer(chunkqueue* cq, buffer* mem){
	if (cq == NULL || mem == NULL)	return;
	if (buffer_string_is_empty(mem))	return;

	chunk* c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;
	buffer_move(c->mem, mem);

	chunkqueue_append_chunk(cq, c);
}


void chunkqueue_append_chunk(chunkqueue* cq, chunk* c){
	if (cq == NULL || c == NULL)	return;

	c->next = NULL;
	if (cq->last != NULL){
		cq->last->next = c;		
	}
	cq->last = c;

	if (cq->first == NULL){
		cq->first = c;
	}

	cq->bytes_in += chunk_remaining_length(c);
}


void chunkqueue_reset(chunkqueue* cq){
	chunk* c;
	if (cq == NULL)	return;

	for (c = cq->first; c; c = c->next){
		chunkqueue_push_unused_chunk(cq,c);
	}
	
	cq->first = cq->last = NULL;
	cq->bytes_in = cq->bytes_out = 0;
	cq->tempdir_idx = 0;
}


void chunkqueue_free(chunkqueue* cq){
	chunk *c, *pc;
	if (cq == NULL)	return;
	for (c = cq->first; c;){
		pc = c;
		c = c->next;
		chunk_free(pc);
	}

	for (c = cq->unused; c;){
		pc = c;
		c = c->next;
		chunk_free(pc);
	}
	free(cq);
}


off_t chunkqueue_length(chunkqueue* cq){
	off_t len = 0;
	chunk* c;

	for (c = cq->first; c; c = c->next){
		len += chunk_remaining_length(c);
	}

	return len;
}


int chunkqueue_is_empty(chunkqueue* cq){
	if (cq == NULL)	return 1;
	return (cq->first == NULL);
}


int chunkqueue_append_mem_to_tempfile(server* srv, chunkqueue* dest, const char* mem, size_t len){
	chunk* dest_c;
	ssize_t written;
	do{
		dest_c = dest->last;
		if (dest_c != NULL && dest_c->type == FILE_CHUNK
			&& dest_c->file.is_temp 
			&& dest_c->file.fd > 0
			&& dest_c->offset == 0){
			
			if (dest_c->file.length > dest->upload_temp_file_size){
				int rc = close(dest_c->file.fd);
				dest_c->file.fd = -1;
				if (rc < 0){
					log_error_write(srv, __FILE__, __LINE__, "sbss",
						"close() temp-file", dest_c->file.name, "failed",
						strerror(errno));
					return -1;
				}
				dest_c = NULL;
			}
		}else{
			dest_c = NULL;
		}

		if (dest_c == NULL && NULL == (dest_c = chunkqueue_get_append_tempfile(dest))){
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"open temp-file failed", strerror(errno));
			return -1;
		}

		written = write(dest_c->file.fd, mem, len);
		if (written == len){
			dest_c->file.length += len;
			dest->bytes_in += len;
			return 0;
		}else if (written >= 0){
			dest_c->file.length += written;
			dest->bytes_in += written;
			len -= written;
			mem += written;
		}else if (errno == EINTR){
		
		}else{
			int retry = (errno == ENOSPC && dest->tempdirs && ++dest->tempdir_idx < dest->tempdirs->used);
			if (!retry){
				log_error_write(srv, __FILE__, __LINE__, "sbss",
					"write() temp-file", dest_c->file.name, "failed", strerror(errno));
			}

			if (0 == chunk_remaining_length(dest_c)){
				chunkqueue_remove_empty_chunks(dest);
			}else{
				int rc = close(dest_c->file.fd);
				dest_c->file.fd = -1;
				if (!rc){
					log_error_write(srv, __FILE__, __LINE__, "sbss",
						"close() temp-file", dest_c->file.name, "failed", strerror(errno));
					return -1;
				}
			}
			if (!retry)	break;
		}

	} while (dest_c);
	return -1;
}


void chunkqueue_get_memory(chunkqueue* cq, char** mem, size_t* len, size_t minsize, size_t allocsize){
	static const size_t RELLOC_MAX_SIZE = 256;
	force_assert(cq != NULL);

	char* dummy_mem;
	size_t dummy_len;
	buffer* b;
	chunk* c;

	if (mem == NULL)	mem = &dummy_mem;
	if (dummy_len == 0)	len = &dummy_len;

	if (0 == minsize)	minsize = 1024;
	if (0 == allocsize)	allocsize = 4096;
	if (allocsize < minsize)	allocsize = minsize;

	if (cq->last != NULL && cq->last->type == MEM_CHUNK){
		size_t have;
		b = cq->last->mem;
		have = buffer_string_space(b);

		if (buffer_string_is_empty(b)){
			buffer_string_prepare_copy(b, allocsize);
			have = buffer_string_space(b);
		}else if (have < minsize && b->size <= RELLOC_MAX_SIZE){
			size_t cur_len = buffer_string_length(b);
			size_t new_size = cur_len + minsize, append;
			if (new_size < allocsize)	new_size = allocsize;

			append = new_size - cur_len;
			if (append >= minsize){
				buffer_string_prepare_append(b, append);
				have = buffer_string_space(b);
			}
		}

		if (have >= minsize){
			*mem = b->ptr + buffer_string_length(b);
			*len = have;
			return;
		}
	}

	c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;
	chunkqueue_append_chunk(cq, c);
	
	b = c->mem;
	buffer_string_prepare_copy(b, allocsize);

	*mem = b->ptr + buffer_string_length(b);
	*len = buffer_string_space(b);

}


void chunkqueue_use_memory(chunkqueue* cq, size_t len){
	buffer* b;
	force_assert(cq != NULL);
	force_assert(cq->last != NULL && cq->last->type == MEM_CHUNK);
	b = cq->last->mem;

	if (len > 0){
		buffer_commit(b, len);
		cq->bytes_in += len;
	}else if (buffer_string_is_empty(b)){
		buffer_reset(b);
	}

}


void chunkqueue_mark_written(chunkqueue* cq, off_t len){
	force_assert(cq != NULL);
	chunk* c;
	off_t c_len;
	off_t written = len;

	for (c = cq->first; c != NULL; c = cq->first){
		c_len = chunk_remaining_length(c);

		if (written == 0 && c_len != 0)	break;

		if (written >= c_len){
			c->offset += c_len;
			written -= c_len;

			cq->first = c->next;
			if (c == cq->last)	cq->last = NULL;

			chunkqueue_push_unused_chunk(cq, c);
		}else{
			c->offset += written;
			written = 0;
			break;
		}
	}
	force_assert(written == 0);
	cq->bytes_out += len;
}


void chunkqueue_remove_finished_chunks(chunkqueue* cq){
	chunk* c;
	force_assert(cq != NULL);
	for (c = cq->first; c; c = cq->first){
		if (0 != chunk_remaining_length(c))	break;
		cq->first = c->next;
		if (c == cq->last)	cq->last = NULL;
		chunkqueue_push_unused_chunk(cq, c);
	}
}


void chunkqueue_steal(chunkqueue* dst, chunkqueue* src, off_t len){
	while (len > 0){
		chunk* c;
		off_t c_len, use;

		c = src->first;
		if (c == NULL)	break;

		c_len = chunk_remaining_length(c);

		if (c_len == 0){
			src->first = c->next;
			if (c == src->last)	src->last = NULL;
			chunkqueue_push_unused_chunk(src, c);
			continue;
		}

		use = len >= c_len ? c_len : len;
		len -= use;

		if (use == c_len){
			src->first = c->next;
			if (c == src->last)	src->last = NULL;
			chunkqueue_append_chunk(dst, c);
		}else{
			switch (c->type){
			case MEM_CHUNK:
				chunkqueue_append_mem(dst, c->mem->ptr + c->offset, use);
				break;
			case FILE_CHUNK:
				chunkqueue_append_file(dst, c->file.name, c->file.start + c->offset, use);
				break;
			}

			c->offset += use;
			force_assert(len == 0);
		}
		src->bytes_out += use;
	}
}


int chunkqueue_steal_with_tempfiles(server* srv, chunkqueue* dst, chunkqueue* src, off_t len){
	while (len > 0){
		chunk* c;
		off_t c_len, use;
		
		c = src->first;
		c_len = chunk_remaining_length(c);

		if (c_len == 0){
			src->first = c->next;
			if (c == src->last)	src->last = NULL;

			chunkqueue_push_unused_chunk(src, c);
			continue;
		}

		use = len >= c_len ? c_len : len;
		len -= use;
		switch (c->type){
		case FILE_CHUNK:
			if (c_len == use){
				src->first = c->next;
				if (c == src->last)	src->last = NULL;

				chunkqueue_append_chunk(dst, c);
			}else{
				chunkqueue_append_file(dst, c->file.name, c->file.start + c->offset, use);
				c->offset += use;
				force_assert(0 == len);
			}
			break;
		case MEM_CHUNK:
			if (0 != chunkqueue_append_mem_to_tempfile(srv, dst, c->mem->ptr + c->offset, use)){
				return -1;
			}

			if (c_len == use){
				src->first = c->next;
				if (c == src->last)	src->last = NULL;
				chunkqueue_push_unused_chunk(src, c);
			}else{
				c->offset += use;
				force_assert(0 == len);
			}
			break;
		}
		src->bytes_out += use;
	}
	return 0;
}


