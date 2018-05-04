#include "mconnections.h"
#include <errno.h>
#include "minet_ntop_cache.h"
#include "mrequest.h"
#include "mresponse.h"


static handler_t connection_handle_fdevent(server* srv, void* context, int revents){

}

static connection* connections_get_new_connection(server* srv){
	connections* conns = srv->conns;
	force_assert(conns != NULL);

	size_t i;

	if (conns->size == 0){
		conns->size += 128;
		conns->ptr = malloc(sizeof(*conns->ptr)*conns->size);
		force_assert(conns->ptr != NULL);
		for (i = 0; i < conns->size; i++){
			conns->ptr[i] = connection_init(srv);
		}
	}else if (conns->used == conns->size){
		conns->size += 128;
		conns->ptr = realloc(conns->ptr, sizeof(*conns->ptr)*conns->size);
		force_assert(conns->ptr != NULL);
		for (i = conns->used; i < conns->size; i++){
			conns->ptr[i] = connection_init(srv);
		}
	}
	connection_reset(srv, conns->ptr[conns->used]);
	conns->ptr[conns->used]->ndx = conns->used;
	return conns->ptr[conns->used++];
}


static int connection_del(server* srv, connection* con){
	connections* conns = srv->conns;
	connection* tmp;
	size_t i;

	if (con == NULL)	return -1;
	if (con->ndx == -1)	return -1;

	buffer_reset(con->uri.authority);
	buffer_reset(con->uri.path);
	buffer_reset(con->uri.query);
	buffer_reset(con->request.orig_uri);

	i = con->ndx;
	if (i != conns->used - 1){
		tmp = conns->ptr[i];
		conns->ptr[i] = conns->ptr[conns->used - 1];
		conns->ptr[conns->used - 1] = tmp;

		conns->ptr[i]->ndx = i;
		conns->ptr[conns->used - 1]->ndx = -1;
	}
	conns->used--;
	con->ndx = -1;
	return 0;
}


static int connection_close(server* srv, connection* con){
	fdevent_event_del(srv->ev, &(con->fde_ndx), con->fd);
	fdevent_unregister(srv->ev, con->fd);

	if (close(con->fd)){
		log_error_write(srv, __FILE__, __LINE__, "sd",
			"warning close:", con->fd);
	}else{
		srv->cur_fds--;
	}
	con->fd = -1;

	if (srv->srvconf.log_state_handling){
		log_error_write(srv, __FILE__, __LINE__, "sd",
			"connection closed for fd", con->fd);
	}

	connection_del(srv, con);
	
	connection_set_state(srv, con, CON_STATE_CONNECT);
	return 0;
}


static int connection_handle_read_state(server* srv, connection* con){
	chunkqueue* cq = con->read_queue;
	chunk* c;
	chunk* last_chunk;
	size_t last_offset;
	int is_closed = 0;
	int is_request_start = chunkqueue_is_empty(cq);

	if (con->is_readable){
		con->read_idle_ts = srv->cur_ts;

		switch (connection_handle_read(srv, con)){
		case -1:
			return -1;
		case -2:
			is_closed = 1;
			break;
		default:
			break;
		}
	}

	chunkqueue_remove_finished_chunks(cq);

	if (con->request_count > 1 && is_request_start){
		con->request_start = srv->cur_ts;
		if (con->conf.high_precision_timestamps)
			log_clock_gettime_realtime(&con->request_start_hp);
	}

	last_chunk = NULL;
	last_offset = 0;

	for (c = cq->first; c; c = c->next){
		size_t i;
		size_t len = buffer_string_length(c->mem) - c->offset;
		const char* b = c->mem->ptr + c->offset;

		for (i = 0; i < len; i++){
			char ch = b[i];

			if (ch == '\r'){
				chunk* cc = c;
				size_t j = i + 1;
				const char head_end[] = "\r\n\r\n";
				int head_end_pos = 1;

				for (; cc; cc = cc->next, j = 0){
					size_t bblen = buffer_string_length(cc->mem) + cc->offset;
					const char* bb = cc->mem->ptr + cc->offset;

					for (; j < bblen; j++){
						ch = bb[j];
						if (ch == head_end[head_end_pos]){
							head_end_pos++;
							if (4 == head_end_pos){
								last_chunk = cc;
								last_offset = j + 1;
								goto found_header_end;
							}else{
								goto reset_rearch;
							}
						}
					}
				}
			}
reset_rearch: ;
		}	
	}
found_header_end:
	if (last_chunk){
		buffer_reset(con->request.request);

		for (c = cq->first; c; c = c->next){
			size_t len = buffer_string_length(c->mem) - c->offset;

			if (c == last_chunk){
				len = last_offset;
			}

			buffer_append_string_len(con->request.request, c->mem->ptr + c->offset, len);
			c->offset += len;
			cq->bytes_out += len;

			if (c == last_chunk)	break;
		}

		connection_set_state(srv, con, CON_STATE_REQUEST_END);
	}else if (is_closed){
		connection_set_state(srv, con, CON_STATE_ERROR);
	}

	if ((last_chunk ? buffer_string_length(con->request.request) : chunkqueue_length(cq))
					> srv->srvconf.max_request_field_size){
		log_error_write(srv, __FILE__, __LINE__, "s", "oversized request-header -> Sending status 431");
		con->http_status = 431;
		con->keep_alive = 0;
		connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
	}

	chunkqueue_remove_finished_chunks(cq);
	return 0;
}


connection* connection_init(server* srv){
	connection* con = calloc(1, sizeof(*con));
	force_assert(con != NULL);

	return con;
}


int connection_reset(server* srv, connection* con){
	plugins_call_connection_reset(srv, con);

	return 0;
}


void connections_free(server* srv){
	connections* conns = srv->conns;
	size_t i;
	for (i = 0; i < conns->used; i++){
		connection* con = conns->ptr[i];
		
		connection_reset(srv, con);

		free(con);
	}
}


connection* connection_accept(server* srv, server_socket* srv_sock){
	int cnt;
	sock_addr addr;
	socklen_t addrlen;

	if (srv->conns->used > srv->srvconf.max_conns)
		return NULL;

	addrlen = sizeof(addr);
	cnt = accept(srv_sock->fd, (struct sockaddr*)&addr, &addrlen);
	
	if (cnt == -1){
		switch (errno){
		case EINTR:
		case EWOULDBLOCK:
		case ECONNABORTED:
			break;
		case EMFILE:
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd", "accept failed", strerror(errno), errno);
			break;
		}
	}else {
		if (addr.plain.sa_family != AF_UNIX){
			network_accept_tcp_nagle_disable(cnt);
		}
		return connection_accepted(srv, srv_sock, &addr, cnt);
	}
	return NULL;
}


connection* connection_accepted(server* srv, server_socket* srv_socket, sock_addr* cnt_addr,int cnt){
	connection* con;
	srv->cur_fds++;

	con = connections_get_new_connection(srv);

	con->fd = cnt;
	con->fde_ndx = -1;
	con->dst_addr = *cnt_addr;

	fdevent_register(srv->ev, con->fd, connection_handle_fdevent, con);
	connection_set_state(srv, con, CON_STATE_REQUEST_START);
	con->connection_start = srv->cur_ts;
	con->srv_socket = srv_socket;
	buffer_copy_string(con->dst_addr_buf, inet_ntop_cache_get_ip(srv, cnt_addr));

	if (-1 == fdevent_fcntl_set_nb_cloexec_sock(srv->ev, con->fd)){
		log_error_write(srv, __FILE__, __LINE__, "ss", "fcntl failed:", strerror(errno));
		connection_close(srv, con);
		return NULL;
	}
	return con;
}



int connection_state_machine(server* srv, connection* con){
	int done, r;
	done = 0;

	if (srv->srvconf.log_state_handling)
		log_error_write(srv, __FILE__, __LINE__, "sds",
		"start at start ", con->fd, connection_get_state(con->state));

	while (done == 0){
		size_t ostate = con->state;

		if (srv->srvconf.log_state_handling){
			log_error_write(srv, __FILE__, __LINE__, "sds",
				"state for fd", con->fd,
				connection_get_state(con->state));
		}
		switch (con->state){
		case CON_STATE_REQUEST_START:
			con->request_start = srv->cur_ts;
			con->read_idle_ts = srv->cur_ts;
			if (con->conf.high_precision_timestamps)
				log_clock_gettime_realtime(&con->request_start_hp);

			con->request_count++;
			con->loops_per_request = 0;

			connection_set_state(srv, con, CON_STATE_READ);
			break;
		case CON_STATE_REQUEST_END:
			buffer_reset(con->uri.authority);
			buffer_reset(con->uri.path);
			buffer_reset(con->uri.query);
			buffer_reset(con->request.orig_uri);

			if (http_request_parse(srv, con)){
				connection_set_state(srv, con, CON_STATE_READ_POST);
				break;
			}
			connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
			break;
		case CON_STATE_READ_POST:
		case CON_STATE_HANDLE_REQUEST:
			switch (r = http_response_prepare(srv, con)){
			case HANDLER_WAIT_FOR_EVENT:
				break;
			case HANDLER_FINISHED:
				break;
			case HANDLER_WAIT_FOR_FD:
				break;
			case HANDLER_COMEBACK:
				break;
			case HANDLER_ERROR:
				break;
			default:
				break;
			}
			break;
		case CON_STATE_RESPONSE_START:
			break;
		case CON_STATE_RESPONSE_END:
		case CON_STATE_ERROR:
			break;
		case CON_STATE_CONNECT:
			break;
		case CON_STATE_CLOSE:
			break;
		case CON_STATE_READ:
			connection_handle_read_state(srv, con);
			break;
		case CON_STATE_WRITE:
			break;
		default:
			break;
		}

		if (done == -1){
			done = 0;
		}else if (ostate == con->state)
			done = 1;
	}

	
}

