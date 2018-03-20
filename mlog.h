#ifndef MLOG_H_
#define MLOG_H_
#include "mbase.h"

int log_error_open(server* srv);
int log_error_close(server* srv);
int log_error_write(server* srv, const char* filename, unsigned int line, const char* fmt, ...);

void openDevNull(int fd);
int write_all(int fd, const void* buf, size_t count);

#endif