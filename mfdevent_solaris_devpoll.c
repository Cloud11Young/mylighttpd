#include "mfdevent.h"

#ifdef USE_SOLARIS_DEVPOLL
#include <sys/devpoll.h>
int fdevent_solaris_devpoll_init(fdevents* ev){
	return 0;
}
#else
int fdevent_solaris_devpoll_init(fdevents* ev){
	UNUSED(ev);
	log_error_write(ev->srv, __FILE__, __LINE__, "s",
		"solaris-devpoll not supported, Try to set server.event_handler=\"select\" or \"poll\"");
	return -1;
}
#endif