#ifndef MSETTINGS_H_
#define MSETTINGS_H_

#define UNUSED(x) ((void)x)
#define BV(x) (1<<x)

typedef enum{
	HANDLER_UNSET,
	HANDLER_GO_ON,
	HANDLER_FINISHED,
	HANDLER_COMEBACK,
	HANDLER_WAIT_FOR_EVENT,
	HANDLER_ERROR,
	HANDLER_WAIT_FOR_FD
}handler_t;

#define MAX_READ_LIMIT (256 * 1024)
#define MAX_WRITE_LIMIT (256 * 1024)

#define BUFFER_MAX_REUSE_SIZE (4 * 1024)
#endif