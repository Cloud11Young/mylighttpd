#ifndef MLOG_H_
#define MLOG_H_
#include "mbase.h"

int open_logfile_or_pipe(server* srv, const char* logfile);

int log_error_open(server* srv);
int log_error_close(server* srv);
int log_error_write(server* srv, const char* filename, unsigned int line, const char* fmt, ...);
int write_all_multiline_buffer(server* srv, const char* filename, unsigned int line, buffer* multiline, const char* fmt, ...);
int log_error_cycle(server* srv);

void openDevNull(int fd);
int write_all(int fd, const void* buf, size_t count);


#endif