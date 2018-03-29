#include <stdlib.h>
#include "mbase.h"
#include "mstat_cache.h"

stat_cache* stat_cache_init(){
	stat_cache* s = malloc(sizeof(*s));
	return s;
}