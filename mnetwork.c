#include "mnetwork.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <errno.h>
#include "mlog.h"
#include "mnetwork_backends.h"
//#include "mbase.h"


typedef enum{
	NETWORK_BACKEND_UNSET,
	NETWORK_BACKEND_WRITE,
	NETWORK_BACKEND_WRITEV,
	NETWORK_BACKEND_SENDFILE
}network_backend_t;

static int network_server_init(server* srv, buffer* host_token, specific_config* s){
	server_socket* srv_socket;
	socklen_t addr_len;
	buffer* b;
	const char* host;
	unsigned int port;
	int err;

	srv_socket = calloc(1, sizeof(*srv_socket));
	srv_socket->addr.plain.sa_family = AF_INET;
	srv_socket->fd = -1;
	srv_socket->fde_ndx = -1;
	
	srv_socket->srv_token = buffer_init();
	buffer_copy_buffer(srv_socket->srv_token, host_token);

	b = buffer_init();
	buffer_copy_buffer(b, host_token);

	host = b->ptr;

	if (host[0] == '/'){
		log_error_write(srv, __FILE__, __LINE__, "s", "ERROR: Unix domain socket are not supported.");
		goto error_free_socket;
	}else{
		size_t len;
		char* sp;

		if (0 == (len = buffer_string_length(b))){
			log_error_write(srv, __FILE__, __LINE__, "s", "value of SERVER[\"socket\"] could not be empty.");
			goto error_free_socket;
		}

		if ((b->ptr[0] == '[' && b->ptr[len - 1] == ']') || (NULL == (sp = strrchr(b->ptr, ':')))){
			port = srv->srvconf.port;
			sp = b->ptr + len;
		}else{
			sp = '\0';
			port = strtol(sp + 1, 0, 10);
		}

		if (b->ptr[0] == '[' && *(sp - 1) == ']'){
			*(sp - 1) = '\0';
			host++;
			s->use_ipv6 = 1;
		}
		if (port == 0 || port > 65535){
			log_error_write(srv, __FILE__, __LINE__, "sd", "port not set or out of range", port);
			goto error_free_socket;
		}
	}

	if (*host == '\0') host = NULL;
#ifdef HAVE_IPV6
	if (s->use_ipv6)
		srv_socket->addr.plain.sa_family = AF_INET6;
#endif
	switch (srv_socket->addr.plain.sa_family){
#ifdef HAVE_IPV6
	case AF_INET6:
		break;
#endif
	case AF_INET:
		memset(&srv_socket->addr, 0, sizeof(srv_socket->addr));
		srv_socket->addr.ipv4.sin_family = AF_INET;
		if (host == NULL){
			srv_socket->addr.ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
		}else{
			struct hostent* he;
			if (NULL == (he = gethostbyname(host))){
				log_error_write(srv, __FILE__, __LINE__, "sds",
					"gethostbyname failed",
					strerror(h_errno));
				goto error_free_socket;
			}
			if (he->h_addrtype != AF_INET){
				log_error_write(srv, __FILE__, __LINE__, "sd", "addr-type != AF_INET", he->h_addrtype);
				goto error_free_socket;
			}
			if (he->h_length != sizeof(struct in_addr)){
				log_error_write(srv, __FILE__, __LINE__, "sd", "addr-length != sizeof( in_addr )", he->h_length);
				goto error_free_socket;
			}
			memcpy(&srv_socket->addr.ipv4.sin_addr.s_addr, he->h_addr_list[0], he->h_length);
		}
		srv_socket->addr.ipv4.sin_port = htons(port);
		addr_len = sizeof(srv_socket->addr.ipv4);
		break;
	default:
		goto error_free_socket;
	}

	if (srv->srvconf.preflight_check){
		err = 0;
		goto error_free_socket;
	}

	if (srv->sockets_disabled){
		goto srv_socket_append;
	}

	{
		if (-1 == (srv_socket->fd = fdevent_socket_nb_cloexec(srv_socket->addr.plain.sa_family, SOCK_STREAM, IPPROTO_TCP))){
			log_error_write(srv, __FILE__, __LINE__, "ss", "socket failed", strerror(errno));
			goto error_free_socket;
		}
#ifdef HAVE_IPV6
#endif
	}

	srv->cur_fds = srv_socket->fd;
	int val=1;
	if (setsockopt(srv_socket->fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0){
		log_error_write(srv, __FILE__, __LINE__, "ss", "setsockopt(SO_REUSEADDR) failed", strerror(errno));
		goto error_free_socket;
	}
	if (setsockopt(srv_socket->fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) < 0){
		log_error_write(srv, __FILE__, __LINE__, "ss", "setsockopt(TCP_NODELAY) failed", strerror(errno));
		goto error_free_socket;
	}
	if (-1 == bind(srv_socket->fd, (struct sockaddr*)&srv_socket->addr, addr_len)){
		log_error_write(srv, __FILE__, __LINE__, "ssds", "can not to bind port", host,port,strerror(errno));
		goto error_free_socket;
	}

	if (-1 == listen(srv_socket->fd, s->listen_backlog)){
		log_error_write(srv, __FILE__, __LINE__, "ss", "listen failed", strerror(errno));
		goto error_free_socket;
	}

	if (s->ssl_enabled){
		log_error_write(srv, __FILE__, __LINE__, "s", "SSL: ssl requested but openssl support is not compiled in");
		goto error_free_socket;
#ifdef TCP_DEFER_ACCEPT
	}else if (s->defer_accept){
		int v = s->defer_accept;
		if (setsockopt(srv_socket->fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &v, sizeof(v)) < 0){
			log_error_write(srv, __FILE__, __LINE__, "ss", "can't set TCP_DEFER_ACCEPT: ", strerror(errno));
			goto error_free_socket;
		}
#endif
	}
srv_socket_append:
	srv_socket->is_ssl = s->ssl_enabled;

	if (srv->srv_sockets.size == 0){
		srv->srv_sockets.size = 4;
		srv->srv_sockets.used = 0;
		srv->srv_sockets.ptr = malloc(srv->srv_sockets.size * sizeof(server_socket*));
		force_assert(srv->srv_sockets.ptr != NULL);
	}else if(srv->srv_sockets.used == srv->srv_sockets.size){
		srv->srv_sockets.size += 4;
		srv->srv_sockets.ptr = realloc(srv->srv_sockets.ptr, srv->srv_sockets.size*sizeof(server_socket*));
		force_assert(srv->srv_sockets.ptr != NULL);
	}

	srv->srv_sockets.ptr[srv->srv_sockets.used++] = srv_socket;

	buffer_free(b);

	return 0;

error_free_socket:
	if (srv_socket->fd != -1){
		if (srv_socket->fde_ndx != -1){
			fdevent_event_del(srv->ev, &srv_socket->fde_ndx, srv_socket->fd);
			fdevent_unregister(srv->ev, srv_socket->fd);
		}
	}
	buffer_free(srv_socket->srv_token);
	free(srv_socket);
	buffer_free(b);
	return err;

}

int network_init(server* srv){
	network_backend_t backend;
	int i;
	struct nb_map{
		network_backend_t nb;
		const char* name;
	}network_backends[] = {
#ifdef USE_SENDFILE
		{NETWORK_BACKEND_SENDFILE,"sendfile"},
#endif
#if defined USE_LINUX_SENDFILE
		{NETWORK_BACKEND_SENDFILE,"linux_sendfile"},
#endif
#if defined USE_FREEBSD_SENDFILE
		{NETWORK_BACKEND_SENDFILE,"freebsd_sendfile"},
#endif
#if defined USE_SOLARIS_SENDFILEV
		{NETWORK_BACKEND_SENDFILE,"solaris_sendfilev"},
#endif
#if defined USE_WRITEV
		{NETWORK_BACKEND_WRITEV,"writev"},
#endif
		{NETWORK_BACKEND_WRITE,"write"},
		{NETWORK_BACKEND_UNSET,NULL}
	};

	buffer* b = buffer_init();
	buffer_copy_buffer(b, srv->srvconf.bindhost);
	buffer_append_string_len(b, CONST_STR_LEN(":"));
	buffer_append_int(b, srv->srvconf.port);
	
	if (0 != network_server_init(srv, b, srv->config_storage[0])){
		buffer_free(b);
		return -1;
	}
	buffer_free(b);

	backend = network_backends[0].nb;

	if (!buffer_string_is_empty(srv->srvconf.network_backend)){
		for (i = 0; network_backends[i].name; i++){
			if (buffer_is_equal_string(srv->srvconf.network_backend, network_backends[i].name, strlen(network_backends[i].name))){
				backend = network_backends[i].nb;
				break;
			}
		}
		if (NULL == network_backends[i].name){
			log_error_write(srv, __FILE__, __LINE__, "sb",
				"server.network_backend has a unknown value",
				srv->srvconf.network_backend);
			return -1;
		}
	}

	switch (backend){
	case NETWORK_BACKEND_WRITE:
		srv->network_backend_write = network_write_chunkqueue_write;
		break;
#ifdef USE_WRITEV
	case NETWORK_BACKEND_WRITEV:
		srv->network_backend_write = network_write_chunkqueue_writev;
		break;
#endif
#ifdef USE_SENDFILE
	case NETWORK_BACKEND_SENDFILE:
		srv->network_backend_write = network_write_chunkqueue_sendfile;
		break;
#endif
	default:
		return -1;
	}

	for (i = 1; i < srv->config_context->used; i++){
		data_config* dc = srv->config_context->data[i];
		specific_config* s = srv->config_storage[i];

		if (COMP_SERVER_SOCKET != dc->comp) continue;
		if (CONFIG_COND_EQ != dc->cond) continue;

		int j;
		for (j = 0; j < srv->srv_sockets.used; j++){
			if (buffer_is_equal(srv->srv_sockets.ptr[j]->srv_token, dc->string))
				break;
		}

		if (j == srv->srv_sockets.used){
			if (0 != network_server_init(srv, dc->string, s)) return -1;
		}
	}
}

int network_close(server* srv){

}

int network_register_fdevents(server* srv){

}