#ifndef MCHUNK_H_
#define MCHUNK_H_

#include "mbuffer.h"
#include "marray.h"


typedef struct chunk{
	enum{ MEM_CHUNK, FILE_CHUNK } type;
	buffer* mem;
	
	struct {
		buffer* name;
		off_t start;
		off_t length;
		
		int fd;

		struct{
			char* start;
			size_t length;
			off_t offset;
		}mmap;

		int is_temp;
	} file;
	
	off_t offset;
	struct chunk* next;
}chunk;

typedef struct chunkqueue{
	chunk* first;
	chunk* last;

	chunk* unused;
	size_t unused_chunks;

	off_t bytes_in, bytes_out;

	array* tempdirs;
	unsigned int upload_temp_file_size;
	unsigned int tempdir_idx;
}chunkqueue;

chunkqueue* chunkqueue_init();
void chunkqueue_set_tempdirs_default(array* tempdirs, unsigned int upload_temp_file_size);
void chunkqueue_append_file(chunkqueue* cq, buffer* fn, off_t offset, off_t len);
void chunkqueue_append_file_fd(chunkqueue* cq, buffer* fn, int fd, off_t offset, off_t len);
void chunkqueue_append_mem(chunkqueue* cq, const char* mem, size_t len);
void chunkqueue_append_buffer(chunkqueue* cq, buffer* mem);
void chunkqueue_prepend_buffer(chunkqueue* cq, buffer* mem);
void chunkqueue_append_chunk(chunkqueue* cq, chunk* c);

struct server;
void chunkqueue_append_mem_to_tempfile(struct server* srv, chunkqueue* cq, const char* mem, size_t len);
void chunkqueue_get_memory(chunkqueue* cq, char** mem, size_t* len, size_t minsize, size_t allocsize);
void chunkqueue_use_memory(chunkqueue* cq, size_t len);
void chunkqueue_mark_written(chunkqueue* cq, off_t len);
void chunkqueue_remove_finished_chunks(chunkqueue* cq);
void chunkqueue_steal(chunkqueue* dest, chunkqueue* src, off_t len);
int chunkqueue_steal_with_tempfiles(struct server* srv, chunkqueue* dst, chunkqueue* src, off_t len);

void chunkqueue_free(chunkqueue* cq);
void chunkqueue_reset(chunkqueue* cq);
off_t chunkqueue_length(chunkqueue* cq);

int chunkqueue_is_empty(chunkqueue* cq);
#endif