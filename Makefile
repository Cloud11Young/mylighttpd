targets=mserver.o mconfigfile.o mlog.o mbuffer.o mplugin.o mfdevent.o mnetwork.o marray.o msplaytree.o mstat_cache.o mchunk.o mnetwork_backends.o mfdevent_select.o
time=$(shell date)
server:$(targets)
	@echo "$(time)"
	gcc -o server $(targets)
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
clean:
	rm -f $(targets)
