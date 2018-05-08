#ifndef M_STREAM_H_
#define M_STREAM_H_

#include <unistd.h>
#include "mbuffer.h"

typedef struct {
	char* start;
	off_t size;
	int mapped;
}stream;

int stream_open(stream* f, const buffer* filename);
int stream_close(stream* f);

#endif