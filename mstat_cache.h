#ifdef MSTAT_CACHE_H_
#define MSTAT_CACHE_H_

#include "mbase.h"

stat_cache* stat_cache_init();
void stat_cache_free(stat_cache* s);

handler_t stat_cache_get_entry(server* srv, connection* con, buffer* name, stat_cache_entry** fce);
handler_t stat_cache_handle_fdevent(server* srv, void* fce, int revent);
int stat_cache_open_rdonly_fstat(server* srv, buffer* name, struct stat* st);

int stat_cache_trigger_cleanup(server* srv);

#endif