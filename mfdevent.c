#include "mfdevent.h"
#include "mbase.h"

fdevents* fdevent_init(server* srv, size_t maxfds, fdevent_handler_t type){
	fdevents* ev;

	ev = calloc(1, sizeof(*ev));
	force_assert(NULL != ev);
	ev->srv = srv;

	ev->fdarray = calloc(maxfds, sizeof(*ev->fdarray));
	force_assert(NULL != ev->fdarray);
	ev->maxfds = maxfds;
	ev->highfd = -1;

	switch (type){
	case FDEVENT_HANDLER_POLL:
		if (0 != fdevent_poll_init(ev)){
			log_error_write(srv, __FILE__, __LINE__, "s",
				"event-handler poll failed");
			goto error;
		}
		return ev;
	case FDEVENT_HANDLER_SELECT:
		if (0 != fdevent_select_init(ev)){
			log_error_write(srv, __FILE__, __LINE__, "s",
				"event-handler select failed");
			goto error;
		}
		return ev;
	case FDEVENT_HANDLER_LINUX_SYSEPOLL:
		if (0 != fdevent_linux_sysepoll_init(ev)){
			log_error_write(srv, __FILE__, __LINE__, "s",
				"event-handler linux sysepoll failed");
			goto error;
		}
		return ev;
	case FDEVENT_HANDLER_SOLARIS_DEVPOLL:
		if (0 != fdevent_solaris_devpoll_init(ev)){
			log_error_write(srv, __FILE__, __LINE__, "s",
				"event-handler solaris-devpoll failed");
			goto error;
		}
		return ev;
	case FDEVENT_HANDLER_SOLARIS_PORT:
		if (0 != fdevent_solaris_port_init(ev)){
			log_error_write(srv, __FILE__, __LINE__, "s",
				"event-handler solaris-eventport failed");
			goto error;
		}
		return ev;
	case FDEVENT_HANDLER_FREEBSD_KQUEUE:
		if (0 != fdevent_freebsd_kqueue_init(ev)){
			log_error_write(srv, __FILE__, __LINE__, "s",
				"event-handler freebsd-kqueue failed");
			goto error;
		}
		return ev;
	case FDEVENT_HANDLER_LIBEV:
		if (0 != fdevent_libev_init(ev)){
			log_error_write(srv, __FILE__, __LINE__, "s",
				"event-handler libev failed");
			goto error;
		}
		return ev;
	case FDEVENT_HANDLER_UNSET:
		break;
	}

error:
	free(ev->fdarray);
	free(ev);
	log_error_write(srv, __FILE__, __LINE__, "s",
		"event-handler is unknown. try to set server.event-handler = \"poll\" or \"select\"");
	return NULL;
}

int fdevent_reset(fdevents* ev){
	if (ev->reset) return ev->reset(ev);
	return 0;
}


int fdevent_open_cloexec(const char* filename, int flags, mode_t mode){

}


void fd_close_on_exec(int fd){
#ifdef FD_CLOEXEC
	if (fd < 0)	return;
	force_assert(-1 != fcntl(fd, F_SETFD, FD_CLOEXEC));
#else
	UNUSED(fd);
#endif
}

int fdevent_fcntl_set(fdevents* ev, int fd){
	return (ev && ev->fcntl_set) ? ev->fcntl_set(ev, fd) : 0;
}

int fdevent_fcntl_set_nb(fdevents* ev, int fd){
	if (ev && ev->fcntl_set)	return ev->fcntl_set(ev, fd);
#ifdef O_NONBLOCK
	return fcntl(fd, F_SETFL, O_NONBLOCK | O_RDWR);
#else
	return 0;
#endif
}

int fdevent_fcntl_set_nb_cloexec(fdevents* ev, int fd){
	fd_close_on_exec(fd);
	return fdevent_fcntl_set_nb(ev, fd);
}

int fdevent_fcntl_set_nb_cloexec_sock(fdevents* ev, int fd){
#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
	return (ev && ev->fcntl_set) ? ev->fcntl_set(ev, fd) : 0;
#else
	return fdevent_fcntl_set_nb_cloexec(ev, fd);
#endif
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

static fdnode* fdnode_init(){
	fdnode* fdn;
	fdn = calloc(1, sizeof(*fdn));
	force_assert(NULL != fdn);
	fdn->fd = -1;
	return fdn;
}

static void fdnode_free(fdnode* fdn){
	if (fdn == NULL)	return;
	free(fdn);
}

void fdevent_event_set(fdevents* ev, int* fde_ndx, int fd, int events){
	if (-1 == fd)	return;
	if (ev->fdarray[fd]->events == events)	return;

	if (ev->event_set) *fde_ndx = ev->event_set(ev, *fde_ndx, fd, events);
	ev->fdarray[fd]->events = events;
}

void fdevent_event_add(fdevents* ev, int* fde_ndx, int fd, int event){
	if (-1 == fd)	return;

	int events = ev->fdarray[fd]->events;
	if ((events & event) || 0 == event)	return;

	events |= event;
	if (ev->event_set) *fde_ndx = ev->event_set(ev, *fde_ndx, fd, events);
	ev->fdarray[fd]->events = events;
}

void fdevent_event_clr(fdevents* ev, int* fde_ndx, int fd, int event){
	int events;
	if (-1 == fd)	return;

	events = ev->fdarray[fd]->events;
	if (!(events & event))	return;

	events &= ~event;
	if (ev->event_set) *fde_ndx = ev->event_set(ev, *fde_ndx, fd, events);
	ev->fdarray[fd]->events = events;
}

void fdevent_event_del(fdevents* ev, int* fde_ndx, int fd){
	if (-1 == fd)	return;
	if (ev->fdarray[fd] <= (fdnode*)0x2)	return;
	if (ev->event_del)	*fde_ndx = ev->event_del(ev, *fde_ndx, fd);
	ev->fdarray[fd]->events = 0;
}


int fdevent_register(fdevents* ev, int fd, fdevent_handler handler, void* ctx){
	fdnode* fdn;
	fdn = fdnode_init();
	fdn->handler = handler;
	fdn->fd = fd;
	fdn->ctx = ctx;
	fdn->handler_ctx = NULL;
	fdn->events = 0;

	ev->fdarray[fd] = fdn;
	return 0;
}

int fdevent_unregister(fdevents* ev, int fd){
	fdnode* fdn;

	if (!ev) return 0;
	fdn = ev->fdarray[fd];
	force_assert(fdn != NULL);
	force_assert(fdn->fd == fd);

	fdnode_free(fdn);
	ev->fdarray[fd] = NULL;
	return 0;
}

int fdevent_poll(fdevents* ev, time_t timeout){
	return (ev && ev->poll) ? ev->poll(ev, timeout) : -1;
}

int fdevent_event_next_fdndx(fdevents* ev, size_t ndx){
	return (ev && ev->event_next_fdndx) ? ev->event_next_fdndx(ev, ndx) : -1;
}

int fdevent_event_get_fd(fdevents* ev, size_t ndx){
	return (ev&& ev->event_get_fd) ? ev->event_get_fd(ev, ndx) : -1;
}

int fdevent_event_get_revent(fdevents* ev, size_t ndx){
	return (ev && ev->event_get_revent) ? ev->event_get_revent(ev, ndx) : -1;
}

fdevent_handler fdevent_get_handler(fdevents* ev, int fd){
	if (ev->fdarray[fd] == NULL)	return NULL;
	if (ev->fdarray[fd]->fd != fd)	return NULL;

	return ev->fdarray[fd]->handler;
}

void* fdevent_get_context(fdevents* ev, int fd){
	if (ev->fdarray[fd] == NULL)	return NULL;
	if (ev->fdarray[fd]->fd != fd)	return NULL;

	return ev->fdarray[fd]->ctx;
}

void fdevent_sched_run(server* srv, fdevents* ev){

}

void fdevent_sched_close(fdevents* ev, int fd, int issock){

}