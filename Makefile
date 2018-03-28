targets=mserver.o mconfigfile.o mlog.o mbuffer.o mplugin.o mfdevent.o mnetwork.o marray.o msplay_tree.o

server:$(targets)
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
msplay_tree.o:msplay_tree.c msplay_tree.h
	gcc -c msplay_tree.c
clean:
	rm -f $(targets)
