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
#include "metag.h"

#ifndef SIZE_MAX
	#define SIZE_MAX ((size_t)~0)
#endif


#ifndef SSIZE_MAX
	#define SSIZE_MAX ((size_t)~0 >> 1)
#endif


typedef enum{
	T_CONFIG_UNSET,
	T_CONFIG_STRING,
	T_CONFIG_INT,
	T_CONFIG_SHORT,
	T_CONFIG_ARRAY,
	T_CONFIG_BOOLEAN,
	T_CONFIG_LOCAL,
	T_CONFIG_DEPRECATED,
	T_CONFIG_UNSUPPORTED
}config_values_type_t;


typedef enum{
	T_CONFIG_SCOPE_UNSET,
	T_CONFIG_SCOPE_SERVER,
	T_CONFIG_SCOPE_CONNECTION
}config_scope_type_t;


typedef struct {
	const char* key;
	void* destination;

	config_values_type_t type;
	config_scope_type_t scope;
}config_values_t;


typedef struct stat_cache_entry{
	buffer* name;
	buffer* etag;

	struct stat st;
	time_t stat_ts;

	char is_symlink;

	int dir_version;

	buffer* content_type;
}stat_cache_entry;


typedef struct stat_cache {
	splay_tree* files;
	buffer* dir_name;

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
	unsigned short defer_accept;
	int listen_backlog;

	unsigned short ssl_enabled;
	buffer* ssl_pemfile;

	unsigned short follow_symlink;

	unsigned int http_parseopts;

	unsigned short force_lowercase_filenames;
	
	array* mimetypes;
}specific_config;


typedef struct server_config{
	unsigned short port;
	buffer* bindhost;

	buffer* modules_dir;
	buffer* pid_file;
	buffer* changeroot;
	buffer* network_backend;
	
	buffer* errorlog_file;
	buffer* breakagelog_file;

	buffer* event_handler;

	unsigned short errorlog_use_syslog;
	unsigned short log_state_handling;
		
	array* modules;
	array* upload_tempdirs;
	unsigned int upload_temp_file_size;

	unsigned short dont_daemonize;
	int max_conns;
	unsigned short preflight_check;

	unsigned short high_precision_timestamps;
	unsigned short http_header_strict;
	unsigned short http_host_strict;
	unsigned short http_host_normalize;

	size_t max_request_field_size;

	enum{
		STAT_CACHE_ENGINE_UNSET,
		STAT_CACHE_ENGINE_NONE,
		STAT_CACHE_ENGINE_SIMPLE,
#ifdef HAVE_FAM_H
		STAT_CACHE_ENGINE_FAM
#endif
	}stat_cache_engine;
}server_config;


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


typedef enum{
	CON_STATE_CONNECT,
	CON_STATE_REQUEST_START,
	CON_STATE_READ,
	CON_STATE_REQUEST_END,
	CON_STATE_READ_POST,
	CON_STATE_HANDLE_REQUEST,
	CON_STATE_RESPONSE_START,
	CON_STATE_WRITE,
	CON_STATE_RESPONSE_END,
	CON_STATE_ERROR,
	CON_STATE_CLOSE
}connection_state_t;

typedef struct request{
	buffer* orig_uri;
	buffer* request;
}request;

typedef struct {
	buffer* authority;
	buffer* path;
	buffer* query;
}request_uri;

typedef struct connection{
	connection_state_t state;

	int in_joblist;

	specific_config conf;

	etag_flags_t etag_flags;

	int ndx;

	int fd;
	int fde_ndx;
	server_socket* srv_socket;
	sock_addr dst_addr;
	buffer* dst_addr_buf;

	time_t connection_start;
	time_t request_start; 
	time_t request_start_hp;
	time_t read_idle_ts;

	int request_count;
	int loops_per_request;
	
	int http_status;

	request request;
	request_uri uri;

	chunkqueue* read_queue;
	chunkqueue* write_queue;

	unsigned short is_readable;
	unsigned short is_writeable;

	int keep_alive;
	int keep_alive_idle;

	off_t bytes_read;
	off_t bytes_write;
}connection;


typedef struct connections{
	connection** ptr;
	size_t used;
	size_t size;


}connections;


typedef struct buffer_plugin{
	void* ptr;
	size_t used;
	size_t size;
}buffer_plugin;


typedef struct server{
	server_socket_array srv_sockets;

	specific_config** config_storage;
	server_config	  srvconf;
	array*			config_context;
	array*			config_touched;

	enum{ ERRORLOG_PIPE, ERRORLOG_FD, ERRORLOG_FILE, ERRORLOG_SYSLOG } errorlog_mode;
	int errorlog_fd;
	buffer* errorlog_buf;

	buffer_plugin plugins;
	void* plugin_slots;

	fdevents* ev;

	int	max_fds;
	int	max_conns;
	int cur_fds;
	int sockets_disabled;

	time_t cur_ts;
	time_t last_generated_debug_ts;
	time_t startup_ts;
	buffer* ts_debug_str;
	gid_t gid;
	uid_t uid;	

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