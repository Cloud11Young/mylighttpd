#include "mfdevent.h"

#ifdef USE_FREEBSD_KQUEUE
int fdevent_libev_init(fdevents* ev){
	return 0;
}
#else
int fdevent_freebsd_kqueue_init(fdevents* ev){
	UNUSED(ev);
	log_error_write(ev->srv, __FILE__, __LINE__, "s",
		"freebsd-kqueue not supported, Try to set server.handler=\"select\" or \"poll\"");
	return -1;
}
#endif