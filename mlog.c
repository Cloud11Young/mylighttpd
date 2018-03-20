#include "mlog.h"
#include <fcntl.h>

int log_error_open(server* srv){

}

int log_error_close(server* srv){

}

int log_error_write(server* srv, const char* filename, unsigned int line, const char* fmt, ...){

}

void openDevNull(int fd){
	close(fd);
	int nfd = open("/dev/null", O_RDWR);
	if (nfd != -1 || nfd != fd){
		dup2(nfd, fd);
		close(nfd);
	}		
}

int write_all(int fd, const void* buf, size_t count){

}