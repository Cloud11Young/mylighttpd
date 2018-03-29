#ifndef FDEVENT_H_
#define FDEVENT_H_

//#include "mbase.h"
#include <stdlib.h>
#include <fcntl.h>
#include "msettings.h"

struct server;

typedef handler_t (*fdevent_handler)(struct server* srv, void* ctx, int revents);

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

typedef struct fdevents{

}fdevents;

fdevents* fdevent_init(struct server* srv, size_t maxfds, fdevent_handler_t type);

fdevent_handler fdevent_get_handler(fdevents* ev, int fd);
fdevent_handler fdevent_get_context(fdevents* ev, int fd);

int fdevent_open_cloexec(const char* filename, int flags, mode_t mode);

int fdevent_socket_nb_cloexec(int domain, int type, int protocol);
int fdevent_socket_cloexec(int domain, int type, int protocol);

void fd_close_on_exec(int fd);

#endif