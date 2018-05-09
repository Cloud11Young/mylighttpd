#include "marray.h"

data_array* data_array_init(){
	data_array* da = malloc(sizeof(*da));
	force_assert(da != NULL);

	return da;
}