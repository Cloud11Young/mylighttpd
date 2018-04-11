#include "mfdevent.h"

#ifdef USE_SOLARIS_PORT
int fdevent_solaris_port_init(fdevents* ev){
	return 0;
}

#else
int fdevent_solaris_port_init(fdevents* ev){
	UNUSED(ev);
	log_error_write(ev->srv, __FILE__, __LINE__, "s",
		"solaris-port not supported, Try to set server.handler=\"select\" or \"poll\"");
	return -1;
}
#endif