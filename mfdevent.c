#include "mfdevent.h"
#include "mbase.h"

fdevents* fdevent_init(server* srv, size_t maxfds, fdevent_handler_t type){

}

int fdevent_open_cloexec(const char* filename, int flags, mode_t mode){

}


void fd_close_on_exec(int fd){

}

int fdevent_socket_nb_cloexec(int domain, int type, int protocol){
#ifdef SOCK_CLOEXEC
	return socket(domain, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
#else
	int fd;
	if (-1 != (fd = socket(domain, SOCK_STREAM, protocol))){
#ifdef O_NONBLOCK
		fcntl(fd, F_SETFD, O_NONBLOCK);
#endif
#ifdef FD_CLOEXEC
		fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
	}
	return fd;
#endif
}