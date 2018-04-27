targets=mserver.o mconfigfile.o mlog.o mbuffer.o mplugin.o mfdevent.o mnetwork.o marray.o msplaytree.o mstat_cache.o mchunk.o mnetwork_backends.o mfdevent_select.o mfdevent_poll.o mfdevent_linux_sysepoll.o mfdevent_solaris_devpoll.o mfdevent_libev.o mfdevent_freebsd_kqueue.o mfdevent_solaris_port.o metag.o mconnections.o minet_ntop_cache.o mconnection_glue.o

time=$(shell date)
server:$(targets)
	@echo "$(time)"
	gcc -o server $(targets) -ldl
server.o:mserver.c mserver.h
	gcc -c server.c
mconfigfile.o:mconfigfile.c mconfigfile.h
	gcc -c mconfigfile.c
mlog.o:mlog.c mlog.h
	gcc -c mlog.c
mbuffer.o:mbuffer.c mbuffer.h
	gcc -c mbuffer.c
mplugin.o:mplugin.c mplugin.h
	gcc -c mplugin.c 
mfdevent.o:mfdevent.c mfdevent.h
	gcc -c mfdevent.c
mnetwork.o:mnetwork.c mnetwork.h
	gcc -c mnetwork.c
marray.o:marray.c marray.h
	gcc -c marray.c
msplaytree.o:msplaytree.c msplaytree.h
	gcc -c msplaytree.c
mstat_cache.o:mstat_cache.c mstat_cache.h
	gcc -c mstat_cache.c
mchunk.o:mchunk.c mchunk.h
	gcc -c mchunk.c
mnetwork_backends.o:mnetwork_backends.c mnetwork_backends.h
	gcc -c mnetwork_backends.c
mfdevent_select.o:mfdevent_select.c mfdevent.h
	gcc -c mfdevent_select.c
mfdevent_poll.o:mfdevent_poll.o mfdevent.h
	gcc -c mfdevent_poll.c
mfdevent_linux_sysepoll.o:mfdevent_linux_sysepoll.c mfdevent.h
	gcc -c mfdevent_linux_sysepoll.c
mfdevent_solaris_devpoll.o:mfdevent_solaris_devpoll.c mfdevent.h
	gcc -c mfdevent_solaris_devpoll.c
mfdevent_solaris_port.o:mfdevent_solaris_port.c mfdevent.h
	gcc -c mfdevent_solaris_port.c
mfdevent_libev.o:mfdevent_libev.c mfdevent.h
	gcc -c mfdevent_libev.c
mfdevent_freebsd_kqueue.o:mfdevent_freebsd_kqueue.c mfdevent.h
	gcc -c mfdevent_freebsd_kqueue.c
metag.o:metag.c metag.h
	gcc -c metag.c -g
mconnections.o:mconnections.c mconnections.h
	gcc -c mconnections.c -g
minet_ntop_cache.o:minet_ntop_cache.c minet_ntop_cache.h
	gcc -c minet_ntop_cache.c -g
mconnection_glue.o:mconnection_glue.c mconnections.h
	gcc -c mconnection_glue.c -g
clean:
	rm -f $(targets)
