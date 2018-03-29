#ifndef MNETWORK_BACKEND_H_
#define MNETWORK_BACKEND_H_

#include "msettings.h"

int network_write_chunkqueue_write(server* srv, connection* con, int fd, chunkqueue* cq, off_t max_bytes);

#if defined(USE_WRITEV)
int network_write_chunkqueue_writev(server* srv, connection* con, int fd, chunkqueue* cq, off_t max_bytes);
#endif

#if defined(USE_SENDFILE)
int network_write_chunkqueue_sendfile(server* srv, connection* con, int fd, chunkqueue* cq, off_t max_bytes);
#endif

#endif