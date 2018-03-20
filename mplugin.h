#ifndef MPLUGIN_H_
#define MPLUGIN_H_

#include "mbase.h"
#include "msettings.h"

int plugins_load(server* srv);

void plugins_free(server* srv);

int plugins_call_init(server* srv);

#endif