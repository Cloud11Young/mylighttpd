#ifndef MNETWORK_H_
#define MNETWORK_H_

#include "mbase.h"

int network_init(server* srv);
int network_close(server* srv);

int network_register_fdevents(server* srv);

#endif