#include "mfdevent.h"
#include <stdlib.h>
#include <errno.h>

#ifdef USE_LINUX_EPOLL
#include <sys/epoll.h>

static int fdevent_linux_sysepoll_free(fdevents* ev){
	close(ev->epoll_fd);
	free(ev->epoll_events);
}

static int fdevent_linux_sysepoll_poll(fdevents* ev, time_t timeout){
	return epoll_wait(ev->epoll_fd, ev->epoll_events, ev->maxfds, timeout);
}

static int fdevent_linux_sysepoll_event_set(fdevents* ev, int fde_ndx, int fd, int events){
	if (fde_ndx > ev->maxfds)	return -1;
	struct epoll_event ep;
	int add = 0;
	memset(&ep, 0, sizeof(ep));

	add = -1 == fde_ndx ? 1 : 0;

	ep.events = 0;
	if (events & FDEVENT_IN) ep.events |= EPOLLIN;
	if (events & FDEVENT_OUT) ep.events |= EPOLLOUT;	
	
	ep.events |= EPOLLERR | EPOLLHUG;
	ep.data.ptr = NULL;
	ep.data.fd = fd;

	if (0 != epoll_ctl(ev->epoll_fd, add ? EPOLL_CTL_ADD : EPOLL_CTL_MOD, fd, &ep)){
		log_error_write(ev->srv, __FILE__, __LINE__, "sss",
			"epoll_ctl failed: ", strerror(errno), " dying");
		SEGFAULT();
		return 0;
	}
	return fd;
}

static int fdevent_linux_sysepoll_event_del(fdevents* ev, int fde_ndx, int fd){
	struct epoll_event ep;

	if (fde_ndx < 0)	return -1;
	
	memset(&ep, 0, sizeof(ep));
	ep.data.ptr = NULL;
	ep.data.fd = fd;

	if (0 != epoll_ctl(ev->epoll_fd, EPOLL_CTL_DEL, fd, &ep)){
		log_error_write(ev->srv, __FILE__, __LINE__, "ss",
			"epoll_ctl delete failed: ", strerror(errno));
		SEGFAULT();
		return 0;
	}
	return -1;
}

static int fdevent_linux_sysepoll_event_next_fdndx(fdevents* ev, size_t ndx){
	UNUSED(ev);
	ndx = ndx < 0 ? 0 : ndx + 1;
	return ndx;
}

static int fdevent_linux_sysepoll_event_get_fd(fdevents* ev, size_t ndx){
	return ev->epoll_events[ndx].fd;
}

static int fdevent_linux_sysepoll_event_get_revent(fdevents* ev, size_t ndx){
	int events = ev->epoll_events[ndx].events;
	int rts = 0;
	if (events & EPOLLIN)	rts |= FDEVENT_IN;
	if (events & EPOLLOUT)	rts |= FDEVENT_OUT;
	if (events & EPOLLERR)	rts |= FDEVENT_ERR;
	if (events & EPOLLHUG)	rts |= FDEVENT_HUG;
	if (events & EPOLLRRI)	rts |= FDEVENT_PRI;

	return rts;
}

int fdevent_linux_sysepoll_init(fdevents* ev){
	ev->type = FDEVENT_HANDLER_LINUX_SYSEPOLL;
#define SET(x)\
	ev->x = fdevent_linux_sysepoll_##x;
	SET(free);
	SET(poll);

	SET(event_set);
	SET(event_del);

	SET(event_next_fdndx);
	SET(event_get_fd);
	SET(event_get_revent);

	if (0 > (ev->epoll_fd=epoll_create(ev->maxfds))){
		log_error_write(ev->srv, __FILE__, __LINE__, "sss",
			"epoll_create failed (", strerror(errno), "), try to set server.event-handler=\"select\" or \"poll\"");
		return -1;
	}

	fd_close_on_exec(ev->epoll_fd);
	ev->epoll_events = malloc(ev->maxfds*sizeof(*ev->epoll_events));
	force_assert(NULL != ev->epoll_events);
	return 0;
}
#else
int fdevent_linux_sysepoll_init(fdevents* ev){
	UNUSED(ev);
	
	log_error_write(ev->srv, __FILE__, __LINE__, "s",
		"linux-sysepoll not support. try to set server.event-handler=\"select\" or \"poll\"");
	return -1;
}
#endif