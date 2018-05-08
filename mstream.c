#include "mstream.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef O_BINARY
	#define O_BINARY 0
#endif


#ifdef O_NONBLOCK
	#define FIFO_NONBLOCK O_NONBLOCK
#else
	#define FIFO_NONBLOCK 0
#endif


int stream_open(stream* f, const buffer* filename){
	struct stat st;
	int fd;

	f->start = NULL;
	f->size = 0;
	f->mapped = 0;

	if (-1 == (fd = open(filename->ptr, O_RDONLY | O_BINARY | FIFO_NONBLOCK))){
		return -1;
	}

	if (-1 == fstat(fd, &st)){
		close(fd);
		return -1;
	}

	if (st.st_size == 0){
		close(fd);
		return 0;
	}

	f->start = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (f->start == MAP_FAILED){
		f->start = malloc(st.st_size);
		if (f->start == NULL || st.st_size != read(fd, f->start, st.st_size)){
			free(f->start);
			f->start = NULL;
			close(fd);
			return -1;
		}
	}else{
		f->mapped = 1;
	}

	close(fd);
	f->size = st.st_size;
	return 0;
}


int stream_close(stream* f){
	if (f->start != NULL){
		if (f->mapped){
			munmap(f->start, f->size);
		}else{
			free(f->start);
		}
		
	}
	f->start = NULL;
	f->size = 0;
	f->mapped = 0;
	return 0;
}