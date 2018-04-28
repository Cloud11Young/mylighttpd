#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include "mlog.h"
#include <fcntl.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

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
	buffer_append_int(s, line);
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

int open_logfile_or_pipe(server* srv, const char* file){
	int fd;
	if (file[0] == '|'){
#ifdef HAVE_FORK
		int to_log_fds[2];

		if (-1 == pipe(to_log_fds))
			return -1;
		
		switch (fork()){
		case 0:
			close(to_log_fds[1]);
			close(STDIN_FILENO);

			if (STDIN_FILENO != to_log_fds[0]){
				if (STDIN_FILENO != dup2(to_log_fds[0], STDIN_FILENO)){
					log_error_write(srv, __FILE__, __LINE__, "ss",
						"dup2 failed: ", strerror(errno));
					exit(-1);
				}
				close(to_log_fds[0]);
			}

#ifdef FD_CLOEXEC
			{
				int i;
				for (i = 3; i < 256; i++)
					close(i);
			}			
#endif
			
			close(STDERR_FILENO);

			execl("/bin/sh", "sh", "-c", file + 1, NULL);
			log_error_write(srv, __FILE__, __LINE__, "sss",
				"spawning log process failed: ", strerror(errno), file + 1);
			exit(-1);
			break;
		case -1:
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"fork failed ", strerror(errno));
			return -1;
		default:
			close(to_log_fds[0]);
			fd = to_log_fds[1];
			break;
		}
#endif
	}else{
		if (-1 == (fd = open(file, O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644))){
			log_error_write(srv, __FILE__, __LINE__, "ssss",
				"open errorlog ",file, " failed ", strerror(errno));
			return -1;
		}
		
		fd_close_on_exec(fd);
	}

	return fd;
}

int log_error_open(server* srv){
#ifdef HAVE_SYSLOG_H
	openlog("mylighttpd", LOG_CONS | LOG_PID, LOG_DAEMON);
#endif
	srv->errorlog_mode = ERRORLOG_FD;
	srv->errorlog_fd = STDERR_FILENO;

	if (srv->srvconf.errorlog_use_syslog){
		srv->errorlog_mode = ERRORLOG_SYSLOG;
	}else if (!buffer_string_is_empty(srv->srvconf.errorlog_file)){
		srv->errorlog_mode = ERRORLOG_FILE;
		const char* sfile = srv->srvconf.errorlog_file->ptr;

		if (-1 == (srv->errorlog_fd = open_logfile_or_pipe(srv, sfile))){
			return -1;
		}
		if (sfile[0] == '|'){
			srv->errorlog_mode = ERRORLOG_PIPE;
		}
	}

	log_error_write(srv, __FILE__, __LINE__, "s", "server start");

	if (srv->errorlog_mode == ERRORLOG_FD && !srv->srvconf.dont_daemonize){
		srv->errorlog_fd = -1;
	}

	if (!buffer_string_is_empty(srv->srvconf.breakagelog_file)){
		int breakage_fd;
		const char* breaklog = srv->srvconf.breakagelog_file->ptr;
		
		if (srv->errorlog_mode == ERRORLOG_FD){
			srv->errorlog_fd = dup(STDERR_FILENO);
			fd_close_on_exec(srv->errorlog_fd);
		}

		if (-1 == (breakage_fd = open_logfile_or_pipe(srv, breaklog)))
			return -1;

		if (breakage_fd != STDERR_FILENO){
			dup2(breakage_fd, STDERR_FILENO);
			close(breakage_fd);
		}
	}else if (!srv->srvconf.dont_daemonize){
		openDevNull(STDERR_FILENO);
	}
}

int log_error_close(server* srv){
	switch (srv->errorlog_mode){
	case ERRORLOG_FD:
	case ERRORLOG_FILE:
	case ERRORLOG_PIPE:
		if (srv->errorlog_fd != -1){
			if (srv->errorlog_fd != STDERR_FILENO){
				close(srv->errorlog_fd);
			}
			srv->errorlog_fd = -1;
		}		
		break;
	case ERRORLOG_SYSLOG:
#ifdef HAVE_SYSLOG_H
		closelog();
#endif
		break;
	}
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

int log_error_cycle(server* srv){
	if (srv->errorlog_mode == ERRORLOG_FILE){
		const char* logfile = srv->srvconf.errorlog_file->ptr;

		int new_fd;
		if (-1 == (new_fd = open_logfile_or_pipe(srv, logfile))){
			log_error_write(srv, __FILE__, __LINE__, "sss",
				"cycling errorlog '", logfile, "' failed ", strerror(errno));
			close(srv->errorlog_fd);
			srv->errorlog_fd = -1;
#ifdef HAVE_SYSLOG_H
			srv->errorlog_mode = ERRORLOG_SYSLOG;
#endif
		}else{
			close(srv->errorlog_fd);
			srv->errorlog_fd = new_fd;
			fd_close_on_exec(srv->errorlog_fd);
		}
	}
	return 0;
}


int log_clock_gettime_realtime(struct timespec* ts){
#ifdef HAVE_CLOCK_GETTIME
	return clock_gettime(CLOCK_REALTIME, ts);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	ts->tv_sec = tv.tv_sec;
	ts->tv_nsec = tv.tv_usec * 1000;
	return 0;
#endif
}