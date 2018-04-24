#ifndef MPLUGIN_H_
#define MPLUGIN_H_

#include "mbase.h"
#include "msettings.h"

#define PLUGIN_DATA		size_t id

typedef struct plugin{
	size_t version;
	buffer* name;

	void*    (* init)(void);
	handler_t (* set_defaults)		(server* srv, void* p_d);
	handler_t (* cleanup)			(server* srv, void* p_d);
	handler_t (* handle_trigger)	(server* srv, void* p_d);
	handler_t (* handle_sighup)		(server* srv, void* p_d);

	handler_t(*handle_uri_clean)(server* srv, connection* con, void* p_d);
	handler_t(*handle_uri_raw)(server* srv, connection* con, void* p_d);
	handler_t(*handle_request_done)(server* srv, connection* con, void* p_d);
	handler_t(*handle_connection_close)(server* srv, connection* con, void* p_d);
	handler_t(*handle_subrequest)(server* srv, connection* con, void* p_d);
	handler_t(*handle_subrequest_start)(server* srv, connection* con, void* p_d);
	handler_t(*handle_response_start)(server* srv, connection* con, void* p_d);
	handler_t(*handle_docroot)(server* srv, connection* con, void* p_d);
	handler_t(*handle_physical)(server* srv, connection* con, void* p_d);
	handler_t(*connection_reset)(server* srv, connection* con, void* p_d);

	void* data;
	void* lib;
}plugin;

int plugins_load(server* srv);
void plugins_free(server* srv);

int plugins_call_init(server* srv);
handler_t plugins_call_set_defaults(server* srv);
handler_t plugins_call_cleanup(server* srv);

handler_t plugins_call_handle_sighup(server* srv);
handler_t plugins_call_handle_trigger(server* srv);

handler_t plugins_call_handle_uri_clean(server* srv,connection* con);
handler_t plugins_call_uri_raw(server* srv, connection* con);
handler_t plugins_call_request_done(server* srv, connection* con);
handler_t plugins_call_connection_close(server* srv, connection* con);
handler_t plugins_call_subrequest(server* srv, connection* con);
handler_t plugins_call_subrequest_start(server* srv, connection* con);
handler_t plugins_call_response_start(server* srv, connection* con);
handler_t plugins_call_docroot(server* srv, connection* con);
handler_t plugins_call_physical(server* srv, connection* con);
handler_t plugins_call_connection_reset(server* srv, connection* con);

#endif