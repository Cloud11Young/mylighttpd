#ifndef MBASE_H_
#define MBASE_H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "mbuffer.h"
#include "mfdevent.h"
#include "marray.h"
#include "msplaytree.h"
#include "mchunk.h"

typedef struct stat_cache {
	splay_tree* files;
	buffer* dirname;
#ifdef HAVE_FAM_H
	splay_tree* dirs;
	FAMConnection fam;
	int fam_fcce_ndx;
#endif
	buffer* hash_key;
}stat_cache;

typedef struct specific_config{
	buffer* document_root;
	unsigned short high_precision_timestamps;

	unsigned short use_ipv6;
	unsigned short ssl_enabled;
	unsigned short defer_accept;
	int listen_backlog;
}specific_config;

typedef struct server_config{
	unsigned short port;
	buffer* bindhost;

	buffer* modules_dir;
	buffer* pid_file;
	buffer* changeroot;
	buffer* network_backend;
	
	int max_conns;
	unsigned short preflight_check;

	unsigned short high_precision_timestamps;
}server_config;

typedef struct connection{
	int in_joblist;
}connection;

typedef struct connections{
	connection** ptr;
	size_t used;
	size_t size;
}connections;


typedef union sock_addr{	
#ifdef HAVE_IPV6
	struct sockaddr_in6 ipv6;
#endif
	struct sockaddr_in ipv4;
#ifdef HAVE_SYS_UN_H
	struct sockaddr_un un;
#endif
	struct sockaddr plain;	
}sock_addr;

typedef struct server_socket{
	sock_addr addr;
	int fd;
	int fde_ndx;
	unsigned short is_ssl;
	buffer* srv_token;
}server_socket;

typedef struct server_socket_array{
	server_socket** ptr;

	size_t used;
	size_t size;
}server_socket_array;

typedef struct server{
	server_socket_array srv_sockets;

	specific_config** config_storage;
	server_config	  srvconf;

	fdevents* ev;

	int	max_fds;
	int	max_conns;
	int cur_fds;
	int sockets_disabled;

	time_t cur_ts;

	gid_t gid;
	uid_t uid;

	array* config_context;
	array* config_touched;

	short int config_unsupported;
	short int config_deprecated;

	buffer* tmp_buf;
	fdevent_handler_t event_handler;

	connections* conns;
	connections* joblist;

	stat_cache* stat_cache;

	int (*network_backend_write)(struct server* srv, connection* conn, int fd, chunkqueue* cq, off_t max_bytes);
}server;

#endif