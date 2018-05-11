targets=mserver.o mconfigfile.o mlog.o mbuffer.o mplugin.o mfdevent.o mnetwork.o marray.o msplaytree.o mstat_cache.o mchunk.o mnetwork_backends.o mfdevent_select.o mfdevent_poll.o mfdevent_linux_sysepoll.o mfdevent_solaris_devpoll.o mfdevent_libev.o mfdevent_freebsd_kqueue.o mfdevent_solaris_port.o metag.o mconnections.o minet_ntop_cache.o mconnection_glue.o mrequest.o mresponse.o mdata_config.o mdata_integer.o mdata_string.o mstream.o configparser.o mvector.o mdata_array.o mconfigfile-glue.o

time=$(shell date)
server:$(targets)
	@echo "$(time)"
	gcc -o server -g $(targets) -ldl
mserver.o:mserver.c mserver.h
	gcc -c mserver.c -g
mconfigfile.o:mconfigfile.c mconfigfile.h
	gcc -c mconfigfile.c -g
mlog.o:mlog.c mlog.h
	gcc -c mlog.c -g
mbuffer.o:mbuffer.c mbuffer.h
	gcc -c mbuffer.c -g
mplugin.o:mplugin.c mplugin.h
	gcc -c mplugin.c -g
mfdevent.o:mfdevent.c mfdevent.h
	gcc -c mfdevent.c -g
mnetwork.o:mnetwork.c mnetwork.h
	gcc -c mnetwork.c -g
marray.o:marray.c marray.h
	gcc -c marray.c -g
msplaytree.o:msplaytree.c msplaytree.h
	gcc -c msplaytree.c -g
mstat_cache.o:mstat_cache.c mstat_cache.h
	gcc -c mstat_cache.c -g
mchunk.o:mchunk.c mchunk.h
	gcc -c mchunk.c -g
mnetwork_backends.o:mnetwork_backends.c mnetwork_backends.h
	gcc -c mnetwork_backends.c -g
mfdevent_select.o:mfdevent_select.c mfdevent.h
	gcc -c mfdevent_select.c -g
mfdevent_poll.o:mfdevent_poll.c mfdevent.h
	gcc -c mfdevent_poll.c -g
mfdevent_linux_sysepoll.o:mfdevent_linux_sysepoll.c mfdevent.h
	gcc -c mfdevent_linux_sysepoll.c -g
mfdevent_solaris_devpoll.o:mfdevent_solaris_devpoll.c mfdevent.h
	gcc -c mfdevent_solaris_devpoll.c -g
mfdevent_solaris_port.o:mfdevent_solaris_port.c mfdevent.h
	gcc -c mfdevent_solaris_port.c -g
mfdevent_libev.o:mfdevent_libev.c mfdevent.h
	gcc -c mfdevent_libev.c -g
mfdevent_freebsd_kqueue.o:mfdevent_freebsd_kqueue.c mfdevent.h
	gcc -c mfdevent_freebsd_kqueue.c -g
metag.o:metag.c metag.h
	gcc -c metag.c -g
mconnections.o:mconnections.c mconnections.h
	gcc -c mconnections.c -g
minet_ntop_cache.o:minet_ntop_cache.c minet_ntop_cache.h
	gcc -c minet_ntop_cache.c -g
mconnection_glue.o:mconnection_glue.c mconnections.h
	gcc -c mconnection_glue.c -g
mrequest.o:mrequest.c mrequest.h
	gcc -c mrequest.c -g
mresponse.o:mresponse.c mresponse.h
	gcc -c mresponse.c -g
mdata_config.o:mdata_config.c marray.h
	gcc -c mdata_config.c -g
mdata_integer.o:mdata_integer.c marray.h
	gcc -c mdata_integer.c -g
mdata_string.o:mdata_string.c marray.h
	gcc -c mdata_string.c -g
mstream.o:mstream.c mstream.h
	gcc -c mstream.c -g
mvector.o:mvector.c mvector.h
	gcc -c mvector.c -g
mdata_array.o:mdata_array.c marray.h
	gcc -c mdata_array.c -g
mconfigfile-glue.o:mconfigfile-glue.c mconfigfile.h
	gcc -c mconfigfile-glue.c -g
configparser.o:configparser.c mconfigfile.h
	gcc -c configparser.c -g

clean:
	rm -f $(targets)
