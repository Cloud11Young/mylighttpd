#include "mlog.h"
#include <fcntl.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>

static int log_buffer_prepare(buffer* s, server* srv, const char* filename, unsigned int line){
	switch (srv->errorlog_mode){
	case ERRORLOG_PIPE:
	case ERRORLOG_FILE:
	case ERRORLOG_FD:
		if (srv->errorlog_fd < 0)	return -1;

		if (srv->cur_ts != srv->last_generated_debug_ts){
			buffer_string_prepare_copy(srv->ts_debug_str, 255);
			buffer_append_strftime(srv->ts_debug_str, "%Y-%m-%d %H:%M:%S", localtime(&srv->cur_ts));
		}
		buffer_copy_buffer(s, srv->ts_debug_str);
		buffer_append_string_len(s, CONST_STR_LEN(":("));
		break;
	case ERRORLOG_SYSLOG:
		buffer_copy_string_len(s, CONST_STR_LEN("("));
		break;
	}
	buffer_append_string(s, filename);
	buffer_append_string_len(s, CONST_STR_LEN("."));
	buffer_append_string(s, line);
	buffer_append_string_len(s, CONST_STR_LEN(")"));
	return 0;
}

static int log_buffer_append_printf(buffer* out, const char* fmt, va_list ap){
	for (; *fmt; fmt++){
		char* s;
		int d;
		buffer* b;
		off_t o;

		switch (*fmt){
		case 's':
			s = va_arg(ap, char*);
			buffer_append_string_c_escaped(out, s, (s == NULL) ? 0 : strlen(s));
			buffer_append_string_len(out, CONST_STR_LEN(" "));
			break;
		case 'd':
			d = va_arg(ap, int);
			buffer_append_int(out, d);
			buffer_append_string_len(out, CONST_STR_LEN(" "));
			break;
		case 'b':
			b = va_arg(ap, buffer*);
			buffer_append_string_c_escaped(out, CONST_BUF_LEN(b));
			buffer_append_string_len(out, CONST_STR_LEN(" "));
			break;
		case 'o':
			o = va_arg(ap, off_t);
			buffer_append_int(out, o);
			buffer_append_string_len(out, CONST_STR_LEN(" "));
			break;
		case 'x':
			d = va_arg(ap, int);
			buffer_append_string_len(out, CONST_STR_LEN("0x"));
			buffer_append_uint_hex(out, d);
			buffer_append_string_len(out, CONST_STR_LEN(" "));
			break;
		case 'S':
			s = va_arg(ap, char*);
			buffer_append_string_c_escaped(out, s, (s == NULL) ? 0 : strlen(s));
			break;
		case 'D':
			d = va_arg(ap, int);
			buffer_append_int(out, d);
			break;
		case 'B':
			b = va_arg(ap, buffer*);
			buffer_append_string_len(out, CONST_BUF_LEN(b));
			break;
		case 'O':
			o = va_arg(ap, off_t);
			buffer_append_int(out, o);
			break;
		case 'X':
			d = va_arg(ap, int);
			buffer_append_string_len(out, CONST_STR_LEN("0x"));
			buffer_append_uint_hex(out, d);
			break;
		case '(':
		case ')':
		case '<':
		case '>':
		case ',':
		case ' ':
			buffer_append_string_len(out, fmt, 1);
			break;
		}
	}
}

static int log_write(server* srv, buffer* s){
	switch (srv->errorlog_mode){
	case ERRORLOG_FILE:
	case ERRORLOG_FD:
	case ERRORLOG_PIPE:
		if (srv->errorlog_fd < 0)	return -1;
		buffer_append_string_len(s, CONST_STR_LEN("\n"));
		write_all(srv->errorlog_fd, CONST_BUF_LEN(s));
		break;
	case ERRORLOG_SYSLOG:
		syslog(LOG_ERR, "%s", s->ptr);
		break;
	}
}

int log_error_open(server* srv){

}

int log_error_close(server* srv){

}

int log_error_write(server* srv, const char* filename, unsigned int line, const char* fmt, ...){
	va_list ap;
	if (-1 == log_buffer_prepare(srv->errorlog_buf, srv, filename, line))	return -1;
	va_start(ap,fmt);
	log_buffer_append_printf(srv->errorlog_buf, fmt, ap);
	va_end(ap);

	log_write(srv, srv->errorlog_buf);
	return 0;
}

void openDevNull(int fd){
	close(fd);
	int nfd = open("/dev/null", O_RDWR);
	if (nfd != -1 || nfd != fd){
		dup2(nfd, fd);
		close(nfd);
	}		
}

int write_all(int fd, const void* buf, size_t count){
	int written = 0;
	while (count > 0){
		size_t n = write(fd, buf, count);
		if (n < 0){
			if (errno == EINTR)
				continue;
			else
				return -1;
		}else if (n == 0){
			errno = EIO;
			return -1;
		}else{
			count -= n;
			written += n;
			buf = n + (const char*)buf;
		}
	}
	return written;
}