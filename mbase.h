#ifndef MBASE_H_
#define MBASE_H_

#include <unistd.h>
#include "mbuffer.h"
#include "mfdevent.h"


typedef struct specific_config{
	buffer* document_root;
	unsigned short high_precision_timestamps;
}specific_config;

typedef struct server_config{
	buffer* modules_dir;
	buffer* pid_file;
	int max_conns;
	unsigned short preflight_check;

	unsigned short high_precision_timestamps;
}server_config;

typedef struct server{
	specific_config** config_storage;
	server_config	  srvconf;
	int				  max_fds;
	int				  max_conns;

	gid_t gid;
	uid_t uid;

	buffer*           tmp_buf;
	fdevent_handler_t event_handler;
}server;

#endif