#include "mconnections.h"
#include <sys/ioctl.h>
#include <errno.h>

static int connection_handle_read_ssl(server* srv, connection* con){

}


int connection_set_state(server* srv, connection* con, connection_state_t state){
	UNUSED(srv);
	
	con->state = state;
	return 0;
}

const char* connection_get_state(connection_state_t state){
	switch (state){
	case CON_STATE_CONNECT:	return "connect";
	case CON_STATE_REQUEST_START:	return "request-start";
	case CON_STATE_READ:	return "read";
	case CON_STATE_HANDLE_REQUEST:	return "handle-request";
	case CON_STATE_REQUEST_END:	return "request-end";
	case CON_STATE_READ_POST:	return "read-post";
	case CON_STATE_RESPONSE_START:	return "response-start";
	case CON_STATE_WRITE:	return "write";
	case CON_STATE_RESPONSE_END:	return "response-end";
	case CON_STATE_CLOSE:	return "close";
	case CON_STATE_ERROR:	return "error";
	default:
		return "unknown";
	}
}


int connection_handle_read(server* srv, connection* con){
	char* mem = NULL;
	size_t mem_len = 0;
	size_t toread = 0;
	ssize_t len;

	if (con->srv_socket->is_ssl){
		return connection_handle_read_ssl(srv, con);
	}

	if (ioctl(con->fd, FIONREAD, &toread) || toread < 4 * 1024){
		toread = 4096;
	}else if (toread > MAX_READ_LIMIT){
		toread = MAX_READ_LIMIT;
	}

	chunkqueue_get_memory(con->read_queue, &mem, &mem_len, 0, toread);

	len = read(con->fd, mem, mem_len);
	chunkqueue_use_memory(con->read_queue, len > 0 ? len : 0);

	if (len < 0){
		con->is_readable = 0;

		switch (errno){
		case EINTR:
			con->is_readable = 1;
			return 0;
		case EAGAIN:
			return 0;
		case ECONNRESET:
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd", "connection-closed read failed", strerror(errno), errno);
		}
		connection_set_state(srv, con, CON_STATE_ERROR);
		return -1;
	}else if (len == 0){
		con->is_readable = 0;
		return -2;
	}else if (len != mem_len){
		con->is_readable = 0;
	}
	con->bytes_read += len;
	return 0;	
}

