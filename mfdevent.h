#ifndef FDEVENT_H_
#define FDEVENT_H_

//#include "mbase.h"
#include <stdlib.h>
#include <fcntl.h>
#include "msettings.h"

struct server;

typedef handler_t (*fdevent_handler)(struct server* srv, void* ctx, int revents);

#define FDEVENT_IN	BV(0)


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

typedef struct fdnode{
	int fd;
	int events;	
	void* ctx;
	void* handler_ctx;
	fdevent_handler handler;
}fdnode;

typedef struct buffer_int{
	int* ptr;

	size_t used;
	size_t size;
}buffer_int;

typedef struct fdevents{
	struct server* srv;
	fdnode** fdarray;
	size_t maxfds;
	size_t highfd;

#ifdef USE_SELECT
	fd_set select_set_read;
	fd_set select_set_write;
	fd_set select_set_error;
	size_t select_max_fd;
#endif
#ifdef USE_POLL
	struct pollfd* pollfds;

	size_t used;
	size_t size;
	buffer_int unused;
#endif
	int(*reset)(struct fdevents* ev);
	int (*event_set)(struct fdevents* ev, int fde_ndx, int fd, int events);
	int (*event_del)(struct fdevents* ev, int fde_ndx, int fd);
}fdevents;

fdevents* fdevent_init(struct server* srv, size_t maxfds, fdevent_handler_t type);
int fdevent_reset(fdevents* ev);

fdevent_handler fdevent_get_handler(fdevents* ev, int fd);
fdevent_handler fdevent_get_context(fdevents* ev, int fd);


int fdevent_open_cloexec(const char* filename, int flags, mode_t mode);

int fdevent_socket_nb_cloexec(int domain, int type, int protocol);
int fdevent_socket_cloexec(int domain, int type, int protocol);

void fdevent_event_set(fdevents* ev, int* fde_ndx, int fd, int events);
void fdevent_event_add(fdevents* ev, int* fde_ndx, int fd, int events);
void fdevent_event_clr(fdevents* ev, int* fde_ndx, int fd, int events);
void fdevent_event_del(fdevents* ev, int* fde_ndx, int fd);

int fdevent_register(fdevents* ev, int fd, fdevent_handler handler, void* ctx);
int fdevent_unregister(fdevents* ev, int fd);

void fd_close_on_exec(int fd);

int fdevent_select_init(fdevents *ev);
int fdevent_poll_init(fdevents *ev);
int fdevent_linux_sysepoll_init(fdevents *ev);
int fdevent_solaris_devpoll_init(fdevents *ev);
int fdevent_solaris_port_init(fdevents *ev);
int fdevent_freebsd_kqueue_init(fdevents *ev);
int fdevent_libev_init(fdevents *ev);


#endif