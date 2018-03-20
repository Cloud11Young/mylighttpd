#ifndef FDEVENT_H_
#define FDEVENT_H_

#include <fcntl.h>

typedef enum{	
	FDEVENT_HANDLER_UNSET,
	FDEVENT_HANDLER_SELECT,
	FDEVENT_HANDLER_POLL,
	FDEVENT_HANDLER_LINUX_SYSEPOLL,
	FDEVENT_HANDLER_SOLARIS_DEVPOLL,
	FDEVENT_HANDLER_SOLARIS_PORT,
	FDEVENT_HANDLER_FREEBSD_KQUEUE,
	FDEVENT_HANDLER_LIBEV
}fdevent_handler_t;

int fdevent_open_cloexec(const char* filename, int flags, mode_t mode);

void fd_close_on_exec(int fd);

#endif