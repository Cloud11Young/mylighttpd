#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include "mserver.h"
#include "mlog.h"
#include "mfdevent.h"
#include "mnetwork.h"
#include "mplugin.h"
#include "marray.h"
#include "msettings.h"

#ifdef HAVE_GETUID
#ifndef HAVE_ISSETUGID

static int l_issetugid(){
	return (geteuid != getuid() || getegid() != getgid())
}

#define issetugid() l_issetugid

#endif
#endif

static server* server_init(){
	return NULL;
}

static void server_free(server* srv){

}

static void remove_pid_file(server* srv, int* pid){
	if (!buffer_string_is_empty(srv->srvconf.pid_file) && 0 <= *pid){
		if (0 != ftruncate(*pid, 0)){
			log_error_write(srv, __FILE__, __LINE__, "sbds",
				"ftruncate failed for:",
				srv->srvconf.pid_file,
				errno,
				strerror(errno));
		}
	}

	if (0 <= *pid){
		close(*pid);
		*pid = -1;
	}

	if (!buffer_string_is_empty(srv->srvconf.pid_file)
		&& buffer_string_is_empty(srv->srvconf.changeroot)){
		if (0 != unlink(srv->srvconf.pid_file->ptr)){
			if (errno != EACCES && errno != EPERM){
				log_error_write(srv, __FILE__, __LINE__, "sbds",
					"unlink failed for: ",
					srv->srvconf.pid_file,
					errno,
					strerror(errno));
			}
		}
	}
}

static volatile sig_atomic_t srv_shutdown = 0;
static volatile sig_atomic_t graceful_shutdown = 0;
static volatile sig_atomic_t handle_sig_alarm = 1;
static volatile sig_atomic_t handle_sig_hup = 0;
static volatile sig_atomic_t forwarded_sig_hup = 0;

#if defined(HAVE_SIGACTION) && defined(SA_SIGINFO)
static volatile siginfo_t last_sigterm_info;
static volatile siginfo_t last_sighup_info;

static void sigaction_handler(int signo, siginfo_t* si, void* context){
	static siginfo_t empty_info;
	if (!si) si = &empty_info;
	switch (signo){
	case SIGTERM:
		srv_shutdown = 1;
		last_sigterm_info = *si;
		break;
	case SIGINT:
		if (graceful_shutdown)
			srv_shutdown = 1;
		else
			graceful_shutdown = 1;
		last_sigterm_info = *si;
		break;
	case SIGALARM:
		handle_sig_alarm = 1;
		break;
	case SIGHUP:
		if (!forwarded_sig_hup){
			handle_sig_hup = 1;
			last_sighup_info = *si;
		}else{
			forwarded_sig_hup = 0;
		}		
		break;
	case SIGCHLD:
		break;
	}
}
#elif defined(HAVE_SIGACTION)
static void signal_handler(int signo){
	switch (signo){
	case SIGTERM:
		srv_shutdown = 1;	break;
	case SIGINT:
		if (graceful_shutdown)	srv_shutdown = 1;
		else graceful_shutdown = 1; break;
	case SIGALARM: handle_sig_alarm = 1;break;
	case SIGHUP: handle_sig_hup = 1;	break;
	}
}
#endif

static int daemonize(){
	pid_t pid;
	int pipefd[2];

#ifdef SIGTTOU
	signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif

	umask(0);
	
	if (pipe(pipefd) < 0)	exit(-1);
	if (0>(pid = fork()))	exit(-1);
	else if (pid > 0){
		size_t bytes;
		char buf;
		close(pipefd[1]);
		do{
			bytes = read(pipefd[0], &buf, 1);
		} while (bytes < 0 && errno == EINTR);
		close(pipefd[0]);

		if (bytes <= 0){
			fputs("daemonize server failed to start; check log for details\n", stderr);
			exit(-1);
		}
		exit(0);
	}
	close(pipefd[0]);

	if (-1 == setsid())	exit(-1);
	signal(SIGHUP, SIG_IGN);
	if (0 > (pid = fork()))	exit(-1);
	if (0 != chdir("/"))	exit(-1);
	fd_close_on_exec(pipefd[1]);
	return pipefd[1];
}

int main(int argc, char* argv[]){
	server* srv = NULL;
	int i_am_root;
	int o;
	int pid_fd = -1, parent_pipe_fd;
	time_t idle_limit;
	int num_child = 0;

	setlocale(LC_TIME, "C");

	if ((srv = server_init()) == NULL){
		fprintf(stderr, "did this really happen?\n");
		exit(-1);
	}

#ifdef HAVE_GETUID
	i_am_root = (getuid() == 0);
#else
	i_am_root = 0;
#endif

	while ((o = getopt(argc, argv, "f:m:i:hvVD1pt")) != -1){
		switch (o){
		case 'f':
			if (srv->config_storage){
				log_error_write(srv, __FILE__, __LINE__, "s",
					"Can only read one config file. Use the include command to use multiple config files");
			}
			if (config_read(srv, optarg)){
				server_free(srv);
				return -1;
			}
			break;
		case 'm':
			buffer_copy_string(srv->srvconf.modules_dir, optarg);
			break;
		case 'i':{
			char* endptr;
			long timeout = strtol(optarg, &endptr, 0);
			if (!*optarg || *endptr || timeout < 0){
			 log_error_write(srv, __FILE__, __LINE__, "ss",
				 "Invalid idle timeout value:", optarg);
			 server_free(srv);
			 return -1;
			}
			idle_limit = (time_t)timeout;
			break;
		}
		}
	}

	if (!srv->config_storage){
		log_error_write(srv, __FILE__, __LINE__, "s",
			"No configuration available. Try use -f option");
		server_free(srv);
		return -1;
	}

	openDevNull(STDIN_FILENO);
	openDevNull(STDOUT_FILENO);

	if (config_set_defaults(srv) != 0){
		log_error_write(srv, __FILE__, __LINE__, "s",
			"setting defaults values failed");
		server_free(srv);
		return -1;
	}

#ifdef HAVE_GETUID
	if (!i_am_root && issetugid()){
		log_error_write(srv, __FILE__, __LINE__, "s",
			"Are you nuts ? Don't apply a SUID bit to this binary");
		server_free(srv);
		return -1;
	}
#endif

	if (buffer_string_is_empty(srv->config_storage[0]->document_root)){
		log_error_write(srv, __FILE__, __LINE__, "s",
			"document-root is not set");
		server_free(srv);
		return -1;
	}

	if (plugins_load(srv)){
		log_error_write(srv, __FILE__, __LINE__, "s",
			"loading plugins finally failed");
		plugins_free(srv);
		server_free(srv);
		return -1;
	}

	/*open pid file before chroot*/
	if (!buffer_string_is_empty(srv->srvconf.pid_file)){
		if (-1 != (pid_fd = fdevent_open_cloexec(srv->srvconf.pid_file->ptr, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))){
			struct stat st;
			if (errno != EEXIST){
				log_error_write(srv, __FILE__, __LINE__, "sbs",
					"opening pid_file failed: ", srv->srvconf.pid_file, strerror(errno));
				server_free(srv);
				return -1;
			}
			if (0 != stat(srv->srvconf.pid_file->ptr, &st)){
				log_error_write(srv, __FILE__, __LINE__, "sbs",
					"stating exist pid_file failed:", srv->srvconf.pid_file, strerror(errno));
				server_free(srv);
				return -1;
			}
			if (!S_ISREG(st.st_mode)){
				log_error_write(srv, __FILE__, __LINE__, "sbs",
					"pid-file exists and isn't a regular:",srv->srvconf.pid_file, strerror(errno));
				return -1;
			}
			if (-1 == (pid_fd = open(srv->srvconf.pid_file->ptr, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))){
				log_error_write(srv, __FILE__, __LINE__, "sbs",
					"open pid-file failed", srv->srvconf.pid_file, strerror(errno));
				return -1;
			}
		}
	}

	if (srv->event_handler == FDEVENT_HANDLER_SELECT){
		srv->max_fds = FD_SETSIZE - 200;
	}else{
		srv->max_fds = 4096;
	}

	if (i_am_root){
		struct group* grp = NULL;
		struct password* pwd = NULL;
		int use_rlimit = 1;

#ifdef HAVE_VALGRIND_VALGRIND_H
		if (RUNNING_ON_VALGRIND)	use_rlimit = 0;
#endif

#ifdef HAVE_GETRLIMIT
		struct rlimit rlim;

		if (0 != getrimit(RLIMIT_NOFILE, &rlim)){
			log_error_write(srv, __FILE__, __LINE__, "s",
				"couldn't get 'max filedescriptors'", strerror(errno));
			return -1;
		}

		if (use_rlimit && srv->srvconf.max_fds){
			rlim.rlim_cur = srv->srvconf.max_fds;
			rlim.rlim_max = srv->srvconf.max_fds;
			if (0 != setrlimit(RLIMIT_NOFILE, &rlim)){
				log_error_write(srv, __FILE__, __LINE__, "ss",
					"couldn't set 'max filedescriptors'", strerror(errno));
				return -1;
			}
		}

		if (srv->event_handler == FDEVENT_HANDLER_SELECT){
			srv->max_fds = rlim.rlim_cur < (rlim_t)FD_SETSIZE - 200 ? rlim.rlim_cur : FD_SETSIZE - 200;
		}
		else{
			srv->max_fds = rlim.rlim_cur;
		}

		if (use_rlimit && srv->srvconf.enable_cores && getrlimit(RLIMIT_CORE, &rlim) == 0){
			rlim.rlim_cur = rlim.rlim_max;
			setrlimit(RLIMIT_CORE, &rlim);
		}
#endif

		if (srv->event_handler == FDEVENT_HANDLER_SELECT){
			if (srv->max_fds > (FD_SETSIZE - 200)){
				log_error_write(srv, __FILE__, __LINE__, "sd",
					"can't raise 'max filedescriptors' above ", FD_SETSIZE - 200,
					"if event-handler is 'select'. Use 'poll' or something else or reduce server.max-fds");
				return -1;
			}
		}
#ifdef HAVE_PWD_H
		if (!buffer_string_is_empty(srv->srvconf.groupname)){
			if (NULL == (grp = getgrnam(srv->srvconf.groupname))){
				log_error_write(srv, __FILE__, __LINE__, "sb",
					"can't find groupname", srv->srvconf.groupname);
				return -1;
			}
		}
		if (!buffer_string_is_empty(srv->srvconf.username)){
			if (NULL != (pwd = getpwnam(srv->srvconf.username))){
				log_error_write(srv, __FILE__, __LINE__, "sb",
					"can't find username", srv->srvconf.username)
			}
			if (pwd->pw_uid == 0){
				log_error_write(srv, __FILE__, __LINE__, "s",
					"I will not set uid to 0");
				return -1;
			}
			if (NULL == grp && NULL == (grp = getgrgid(pwd->pw_gid))){
				log_error_write(srv, __FILE__, __LINE__, "sd",
					"can't find group id", pwd->pw_gid);
				return -1;
			}
		}
		if (NULL != grp){
			if (grp->gr_gid == 0){
				log_error_write(srv, __FILE__, __LINE__, "s",
					"I will not set gid to 0");
				return -1;
			}
		}
#endif
		if (network_init(srv) != 0){
			plugins_free(srv);
			server_free(srv);
			return -1;
		}
#ifdef HAVE_PWD_H
	
		if (NULL != grp){
			if (-1 == setgid(grp->gr_gid)){
				log_error_write(srv, __FILE__, __LINE__, "ss",
					"set gid failed", strerror(errno));
				return -1;
			}
			if (-1 == setgroups(0, NULL)){
				log_error_write(srv, __FILE__, __LINE__, "ss", "set groups failed", strerror(errno));
				return -1;
			}
			if (!buffer_string_is_empty(srv->srvconf.username)){
				initgroups(srv->srvconf.username->ptr, grp->gr_gid);
			}
		}
#endif

#ifdef HAVE_CHROOT
		if (!buffer_string_is_empty(srv->srvconf.changeroot)){
			if (-1 == chroot(srv->srvconf.changeroot->ptr)){
				log_error_write(srv, __FILE__, __LINE__, "ss", "change root failed", strerror(errno));
				return -1;
			}
			if (-1 == chdir("/")){
				log_error_write(srv, __FILE__, __LINE__, "ss", "change dir failed", strerror(errno));
				return -1;
			}
		}
#endif

#ifdef HAVE_PWD_H
		/*drop root privs*/
		if (NULL != pwd){
			if (-1 == setuid(pwd->pw_uid)){
				log_error_write(srv, __FILE__, __LINE__, "ss", "setuid failed", strerror(errno));
				return -1;
			}
		}
#endif
#if defined(HAVE_SYS_PRCTL_H) && defined(PR_SET_DUMPABLE)
		if (srv->srvconf.enable_cores){
			prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
		}
#endif
	}else{
#ifdef HAVE_GETRLIMT
		if (-1 == getrlimt(RLIMIT_NOFILE, &rlim)){
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"couldn't get 'max filedescriptors' ", strerror(errno));
			return -1;
		}

		if (srv->srvconf.max_fds && srv->srvconf.max_fds < rlim.rlim_max){
			rlim.rlim_cur = srv->srvconf.max_fds;
			if (-1 == setrlimit(RLIMIT_NOFILE, &rlim)){
				log_error_write(srv, __FILE__, __LINE__, "ss",
					"couldn't set 'max filedescriptors'", strerror(errno));
				return -1;
			}
		}

		if (srv->event_handler == FDEVENT_HANDLER_SELECT){
			srv->max_fds = rlim.rlim_cur < (rlim_t)FD_SETSIZE - 200 ? rlim.rlim_cur : FD_SETSIZE - 200;
		}else{
			srv->max_fds = rlim.rlim_cur;
		}

		if (srv->srvconf.enable_cores && getrlimit(RLIMIT_CORE, &rlim) == 0){
			rlim.rlim_cur = rlim.rlim_max;
			setrlimit(RLIMIT_CORE, &rlim);
		}
#endif
		if (srv->event_handler == FDEVENT_HANDLER_SELECT){
			if (srv->max_fds > FD_SETSIZE - 200){
				log_error_write(srv, __FILE__, __LINE__, "sds",
					"can't raise max filedescriptors above ", FD_SETSIZE - 200,
					"if event-handler is 'select'. Use 'poll' or something else or reduce server.max_fds");
				return -1;
			}
		}

		if (network_init(srv)!=0){
			plugins_free(srv);
			server_free(srv);
			return -1;
		}
	}

	/*set max-conns*/
	if (srv->srvconf.max_conns > srv->max_fds / 2){
		log_error_write(srv, __FILE__, __LINE__, "sdd", "can't have more connections than fds/2", srv->srvconf.max_conns, srv->max_fds);
		srv->max_conns = srv->max_fds / 2;
	}else if (srv->srvconf.max_conns){
		srv->max_conns = srv->srvconf.max_conns;
	}else{
		srv->max_conns = srv->max_fds / 3;
	}

	if (plugins_call_init(srv) != HANDLER_GO_ON){
		log_error_write(srv, __FILE__, __LINE__, "s",
			"Initialization of plugins failed. Going down");
		plugins_free(srv);
		network_close(srv);
		server_free(srv);
		return -1;
	}

#ifdef HAVE_FORK
	if (srv->srvconf.dont_daemonize == 0){
		parent_pipe_fd = daemonize();
	}
#endif

#ifdef HAVE_SIGACTION
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_signal = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);

	sigemptyset(&act.sa_mask);
#if defined(SA_SIGINFO)
	act.sa_flags = SA_SIGINFO;
	act.sa_sigaction = sigaction_handler;
#else
	act.sa_flags = 0;
	act.sa_signal = signal_handler;
#endif
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);	
	sigaction(SIGALARM, &act, NULL);
	sigaction(SIGHUP, &act, NULL);

	act.sa_flags |= SA_RESTART | SA_NOCLDSTP;
	sigaction(SIGCHLD, &act, NULL);
#elif defined(HAVE_SIGNAL)
	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGALARM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGCHLD, signal_handler);
#endif

#ifdef USE_ALARM
	signal(SIGALARM, signal_handler);
	struct itimerval interval;
	interval.it_interval.tv_sec = 1;
	interval.it_interval.tv_usec = 0;
	interval.it_value.tv_sec = 1;
	interval.it_value.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &interval, NULL) == -1){
		log_error_write(srv, __FILE__, __LINE__, "s", "setting timer failed");
		return -1;
	}

	getitimer(ITIMER_REAL, &interval);
#endif

	srv->gid = getgid();
	srv->uid = getuid();

	if (pid_fd != -1){
		buffer_copy_int(srv->tmp_buf, getpid());
		buffer_append_string_len(srv->tmp_buf, CONST_STR_LEN("\n"));
		if (-1 == write_all(pid_fd, CONST_BUF_LEN(srv->tmp_buf))){
			log_error_write(srv, __FILE__, __LINE__, "ss", "Couldn't write pid file",strerror(errno));
			server_free(srv);
			return -1;
		}
	}

	if (!srv->srvconf.preflight_check && -1 == log_error_open(srv)){
		log_error_write(srv, __FILE__, __LINE__, "s", "Opening errorlog failed.Going down");
		plugins_free(srv);
		network_close(srv);
		server_free(srv);
		return -1;
	}

	if (HANDLER_GO_ON != plugins_call_set_defaults(srv)){
		log_error_write(srv, __FILE__, __LINE__, "s", "Configuration of plugins failed. Going down");
		plugins_free(srv);
		network_close(srv);
		server_free(srv);
		return -1;
	}

	srv->config_storage[0]->high_precision_timestamps = srv->srvconf.high_precision_timestamps;


	/*dump unused config-key*/
	size_t i;
	for (i = 0; i < srv->config_context->used; i++){
		array* config = ((data_config*)srv->config_context->data[i])->value;
		size_t j = 0;
		for (j = 0; config && j < config->used; i++){
			data_unset* du = config->data[j];
			if (strncmp(du->key->ptr, "var.", sizeof("var.") - 1) == 0)
				continue;
			if(NULL == array_get_element(srv->config_touched,du->key->ptr)){
				log_error_write(srv, __FILE__, __LINE__, "sbs",
					"WARNING: unknown config-key: ", du->key,
					"(ignored)");
			}
		}
	}

	if (srv->config_unsupported){
		log_error_write(srv, __FILE__, __LINE__, "s",
			"Configuration contains unsupported keys. Going down");
	}
	if (srv->config_deprecated){
		log_error_write(srv, __FILE__, __LINE__, "s",
			"Configuration contains deprecated keys. Going down");
	}

	if (srv->config_unsupported || srv->config_deprecated){
		plugins_free(srv);
		network_close(srv);
		server_free(srv);
		return -1;
	}

	if (srv->srvconf.preflight_check){
		plugins_free(srv);
		network_close(srv);
		server_free(srv);
		exit(0);
	}

#ifdef HAVE_FORK
	if (srv->srvconf.dont_daemonize == 0){
		if (0 > write(parent_pipe_fd, "", 1))	return -1;
		close(parent_pipe_fd);
	}
	
	if (idle_limit && srv->srvconf.max_worker){
		srv->srvconf.max_worker = 0;
		log_error_write(srv, __FILE__, __LINE__, "s",
			"server idle time limit command line option disables server.max_worker config file option.");
	}

	num_child = srv->srvconf.max_worker;
	if (num_child > 0){
		int child = 0;
		while (!child && !srv_shutdown && !graceful_shutdown){
			if (num_child > 0){
				switch (fork()){
				case -1:
					return -1;
				case 0:
					child = 1;
					break;
				default:
					num_child--;
				}
			}else{
				int status;
				if (-1 != wait(status)){
					num_child++;
				}else{
					switch (errno){
					case EINTR:
						if (handle_sig_hup){
							handle_sig_hup = 0;
							log_error_cycle(srv);
							if (!forwarded_sig_hup && 0 != srv->srvconf.max_worker){
								forwarded_sig_hup = 1;
								kill(0, SIGHUP);
							}
						}
						break;
					default:
						break;
					}
				}
			}
		}
		if (!child){
			if (srv_shutdown){
				kill(0, SIGINT);
			}else if (graceful_shutdown){
				kill(0, SIGTERM);
			}

			remove_pid_file(srv, &pid_fd);
			log_error_close(srv);
			network_close(srv);
			connections_free(srv);
			plugins_free(srv);
			server_free(srv);
			return 0;
		}

		if (0 <= *pid_fd){
			close(*pid_fd);
			*pid_fd = -1;
		} 

		buffer_reset(srv->srvconf.pid_file);
		//li_rand_reseed();
	}
#endif

	if (NULL == (srv->ev = fdevent_init(srv, srv->max_fds + 1, srv->event_handler))){
		log_error_write(srv, __FILE__, __LINE__, "s",
			"fdevent_init failed");
		return -1;
	}

	if (0 != network_register_fdevents(srv)){
		plugins_free(srv);
		network_close(srv);
		server_free(srv);
		return -1;
	}

	if (NULL == (srv->stat_cache = stat_cache_init())){
		log_error_write(srv, __FILE__, __LINE__, "s",
			"stat-cache could not be setup, dieing.");
		return -1;
	}

	/*get the current number of fds*/
	srv->cur_fds = open("/dev/null", O_RDONLY);	
	close(srv->cur_fds);

	for (i = 0; i < srv->srv_sockets.used; i++){
		server_socket* srv_socket = srv->srv_sockets.ptr[i];
		if (srv->sockets_disabled) continue;
		if (-1 == fdevent_fcntl_set_nb_cloexec_sock(srv->ev, srv_socket->fd)){
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"fcntl failed ", strerror(errno));
			return -1;
		}
	}

	/*main loop*/
	while (!srv_shutdown){
		time_t min_ts = 0;
		if (handle_sig_hup){
			handle_sig_hup = 0;
			handler_t r;
			switch (r = plugins_call_handle_sighup(srv)){
			case HANDLER_GO_ON:
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sd", "sighup-handler return with an error", r);
				break;
			}

			if (-1 == log_error_cycle(srv)){
				log_error_write(srv, __FILE__, __LINE__, "s", "cycle errorlog failed, dying");
				return -1;
			}else{
#ifdef HAVE_SIGACTION
				log_error_write(srv, __FILE__, __LINE__, "sdsd",
					"logfiles cycled UID = ",
					last_sighup_info.si_uid,
					"PID =",
					last_sighup_info.si_pid);
#else
				log_error_write(srv, __FILE__, __LINE__, "s",
					"logfiles cycled");
#endif
			}
		}

		if (handle_sig_alarm){
#ifdef USE_ALARM
			handle_sig_alarm = 0;
#endif
			
			min_ts = time(NULL);
			if (min_ts != srv->cur_ts){
				srv->cur_ts = min_ts;
				
				connections* conns = srv->conns;
				handler_t r;
				switch (r = plugins_call_handle_trigger(srv)){
				case HANDLER_GO_ON:
					break;
				case HANDLER_ERROR:
					log_error_write(srv, __FILE__, __LINE__, "s", "one of the triggers failed.");
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "d", r);
					break;
				}

				//if (idle_limit && idle_limit < min_ts - last_active_ts && !graceful_shutdown){
				
			//	}
				stat_cache_trigger_cleanup(srv);

				/*check all connections for timeout*/
// 				for (i = 0; i < conns->used; i++){
// 					
// 				}
			}
		}
		int n;
		if ((n = fdevent_poll(srv->ev, 1000)) > 0){
			int revents;
			int fd_ndx = -1;
			int fd;
			do{
				fdevent_handler handler;
				void* context;

				fd_ndx = fdevent_event_next_fdndx(srv->ev, fd_ndx);
				if (fd_ndx == -1)	break;
				revents = fdevent_event_get_revent(srv->ev, fd_ndx);
				fd = fdevent_event_get_fd(srv->ev, fd_ndx);
				handler = fdevent_get_handler(srv->ev, fd);
				context = fdevent_get_context(srv->ev, fd);
				if (handler != NULL)
					(*handler)(srv, context, revents);
			} while (--n > 0);

			fdevent_sched_run(srv, srv->ev);
		}else if (n < 0 && errno != EINTR){
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"fdevent-poll failed ", strerror(errno));
		}

		for (i = 0; i < srv->joblist->used; i++){
			connection* conn = srv->joblist->ptr[i];
			connection_state_machine(srv, conn);
			conn->in_joblist = 0;
		}
		srv->joblist->used = 0;
	}

	if (0 == graceful_shutdown){
		remove_pid_file(srv, &pid_fd);
	}

	if (2 == graceful_shutdown){
		log_error_write(srv, __FILE__, __LINE__, "s", "Server stopped after idle timeout");
	}
	else{
#ifdef HAVE_SIGACTION
		log_error_write(srv, __FILE__, __LINE__, "sdsd",
			"server stopped by UID=",
			last_sigterm_info.si_uid,
			"PID=",
			last_sigterm_info.si_pid);
#else
		log_error_write(srv, __FILE__, __LINE__, "s", "server stopped");
#endif
	}

	log_error_close(srv);
	network_close(srv);
	connections_free(srv);
	plugins_free(srv);
	server_free(srv);
	return 0;
}
