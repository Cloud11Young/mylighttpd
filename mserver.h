#ifndef MSERVER_H_
#define MSERVER_H_

#include "mbase.h"

int config_read(server* srv, const char* fn);
int config_set_defaults(server* srv);

#endif