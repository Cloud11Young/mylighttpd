#include "../mbuffer.h"
#include <stdio.h>

int main(int argc, char** argv){
	buffer* b = buffer_init();
	buffer_append_uint_hex(b, 1024);
	printf("%s\n", b->ptr);
}
