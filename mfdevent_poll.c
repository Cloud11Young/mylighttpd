#include "mfdevent.h"

#ifdef USE_POLL

static int fdevent_poll_free(fdevents* ev){
	free(ev->pollfds);
	if (ev->unused.ptr) free(ev->unused.ptr);
}

static int fdevent_poll_poll(fdevents* ev, int timeout_ms){
	
	return poll(ev->pollfds, ev->used, timeout_ms);
}

static int fdevent_poll_event_set(fdevents* ev, int fde_ndx, int fd, int events){
	int pevents = 0;
	if (events & FDEVENT_IN) pevents |= POLLIN;
	if (events & FDEVENT_OUT) pevents |= POLLOUT;

	if (-1 != fde_ndx){
		if (ev->pollfds[fde_ndx].fd == fd)
			ev->pollfds[fde_ndx].events = pevents;
		return fde_ndx;
	}
	
	if (ev->unused.used > 0){
		int k = ev->unused.ptr[--ev->unused.used];

		ev->pollfds[k].fd = fd;
		ev->pollfds[k].events = pevents;
		return k;
	}else{
		if (ev->size == 0){
			ev->size += 16;
			ev->pollfds = calloc(ev->size, sizeof(*ev->pollfds));
			force_assert(NULL != ev->pollfds);
		}else if (ev->used == ev->size){
			ev->size += 16;
			ev->pollfds = realloc(ev->pollfds, ev->size*sizeof(*ev->pollfds));
			force_assert(NULL != ev->pollfds);
		}
		
		ev->pollfds[ev->used].fd = fd;
		ev->pollfds[ev->used].events = pevents;
		return ev->used++;
	}
}

static int fdevent_poll_event_del(fdevents* ev, int fde_ndx, int fd){
	if (fde_ndx < 0)	return -1;

	if (fde_ndx > ev->used){
		log_error_write(srv, __FILE__, __LINE__, "SdD",
			"del! out of range ", fde_ndx, (int)ev->used);
		SEGFAULT();
	}
	
	if (ev->pollfds[fde_ndx].fd == fd){
		ev->pollfds[fde_ndx].fd = -1;
		ev->pollfds[fde_ndx].events = 0;
	
		if (ev->unused.size == 0){
			ev->unused.size += 16;
			ev->unused.ptr = calloc(ev->unused.size, sizeof(*ev->unused.ptr));
			force_assert(NULL != ev->unused.ptr);
		}
		else if (ev->unused.used == ev->unused.size){
			ev->unused.size += 16;
			ev->unused.ptr = realloc(ev->unused.ptr, ev->unused.size*sizeof(*ev->unused.ptr));
			force_assert(NULL != ev->unused.ptr);
		}

		ev->unused.ptr[ev->unused.used++] = fde_ndx;
	}else{
		log_error_write(srv, __FILE__, __LINE__, "SdD",
			"del! ", ev->pollfds[fde_ndx].fd, fd);
		SEGFAULT();
	}
	return -1;
}

static int fdevent_poll_event_get_revent(fdevents* ev, size_t ndx){
	if (ndx > ev->used){
		log_error_write(srv, __FILE__, __LINE__, "sii",
			"dying because: event: ", (int)ndx, (int)ev->used);
		SEGFAULT();
		return 0;
	}
	if (ev->pollfds[ndx].revents & POLLNVAL){
		SEGFAULT();
	}

	int r = 0;
	int poll_r = ev->pollfds[ndx].revents;
	if (poll_r & POLLIN) r |= FDEVENT_IN;
	if (poll_r & POLLOUT) r |= FDEVENT_OUT;
	if (poll_r & POLLERR) r |= FDEVENT_ERR;
	if (poll_r & POLLHUP) r |= FDEVENT_HUP;
	if (poll_r & POLLNVAL) r |= FDEVENT_NVAL;
	if (poll_r & POLLPRI) r |= FDEVENT_PRI;

	return r;

}

static int fdevent_poll_event_get_fd(fdevents* ev, size_t ndx){
	if (ndx < 0)	return -1;
	if (ndx > ev->used){
		log_error_write(ev->srv, __FILE__, __LINE__, "s",
			"get-fd! out of range.");
		SEGFAULT();
	}

	return ev->pollfds[ndx].fd;
}

static int fdevent_poll_event_next_fdndx(fdevents* ev, size_t ndx){
	ndx = ndx < 0 ? 0 : ndx + 1;
	for (; ndx < ev->used;ndx++)
	if (ev->pollfds[ndx].revents)	return ndx;

	return -1;
}

int fdevent_poll_init(fdevents* ev){
	force_assert(NULL != ev);
	ev->type = FDEVENT_HANDLER_POLL;
#define SET(x)\
	ev->x = fdevent_poll_##x;

	SET(free);
	SET(poll);

	SET(event_set);
	SET(event_del);
	
	SET(event_get_revent);
	SET(event_get_fd);
	SET(event_next_fdndx);

	return 0;
}

#else
int fdevent_poll_init(fdevents* ev){
	UNUSED(ev);
	return -1;
}
#endif