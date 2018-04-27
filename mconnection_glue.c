#include "mconnections.h"

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