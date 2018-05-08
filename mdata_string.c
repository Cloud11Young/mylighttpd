#include "marray.h"

data_string* data_string_init(){
	data_string* ds = calloc(1, sizeof(*ds));
	force_assert(ds != NULL);

	return ds;
}