#include <stdio.h>

int main(int argc,char** argv){
#line 4 "test_macro_line.c"
	printf("line: %d\n", __LINE__);
	printf("file: %s\n", __FILE__);
}