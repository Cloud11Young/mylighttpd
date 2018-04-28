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


void chunkqueue_append_mem_to_tempfile(server* srv, chunkqueue* dest, const char* mem, size_t len){
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
			int retry = (errno == ENOSPC && dest->tempdirs && ++dest->tempdir_idx < dest->tempdirs.used);
			if (!retry){
				log_error_write(srv, __FILE__, __LINE__, "sbss",
					"write() temp-file", dest_c->file.name, "failed", strerror(errno));
			}

			if (0 == chunk_remaining_length(dest_c)){
				chunkqueue_remove_empty_chunks(dest);
			}
			else{
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

}


void chunkqueue_use_memory(chunkqueue* cq, size_t len){


}


void chunkqueue_mark_written(chunkqueue* cq, off_t len){

}


void chunkqueue_remove_finished_chunks(chunkqueue* cq){

}


void chunkqueue_steal(chunkqueue* dst, chunkqueue* src, off_t len){

}


void chunkqueue_steal_with_tempfiles(server* srv, chunkqueue* dst, chunkqueue* src, off_t len){

}


