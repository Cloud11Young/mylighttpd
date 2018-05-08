#include "marray.h"

data_integer* data_integer_init(){
	data_integer* di = calloc(1, sizeof(*di));
	force_assert(di != NULL);

	return di;
}