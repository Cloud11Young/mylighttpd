#include "marray.h"

data_config* data_config_init(){
	data_config* dc = calloc(1, sizeof(*dc));
	force_assert(dc != NULL);

	return dc;
}