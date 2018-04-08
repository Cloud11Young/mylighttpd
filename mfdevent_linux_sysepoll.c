#include "mfdevent.h"

#ifdef USE_LINUX_EPOLL
int fdevent_linux_sysepoll_init(fdevents* ev){

}
#else
int fdevent_linux_sysepoll_init(fdevents* ev){
	UNUSED(ev);
	return 0;
}
#endif