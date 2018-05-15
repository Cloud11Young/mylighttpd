#include "mconfigfile.h"

#include <stdio.h>
#include <errno.h>
#include "mbase.h"
#include "mstream.h"
#include "configparser.h"
#include "mrequest.h"

typedef struct{
	int foo;
	int bar;

	const buffer* source;
	const char* input;
	size_t offset;
	size_t size;

	int line_pos;
	int line;

	int in_key;
	int in_brace;
	int in_cond;
}tokenizer_t;


static int tokenizer_init(tokenizer_t* t, const buffer* source, const char* input, size_t size){
	t->source = source;
	t->input = input;
	t->size = size;
	t->offset = 0;
	t->line_pos = 1;
	t->line = 1;

	t->in_key = 1;
	t->in_brace = 0;
	t->in_cond = 0;
}


static void config_init(server* srv,config_t* config){
	force_assert(config != NULL);
	config->srv = srv;
	config->ok = 1;
	vector_config_weak_init(&config->configs_stack);
	config->basedir = buffer_init();
}


static void config_free(config_t* config){
	vector_config_weak_free(&config->configs_stack);
	buffer_free(config->basedir);
}


static int config_skip_comment(tokenizer_t* t){
	int i;
	force_assert(t->input[t->offset] == '#');
	for (i = 1; t->input[t->offset + i] &&
		(t->input[t->offset + i] != '\n' && t->input[t->offset + i] != '\r'); i++);
	t->offset += i;
	return i;
}


static int config_skip_newline(tokenizer_t* t){
	int i = 1;
	force_assert(t->input[t->offset] == '\r' || t->input[t->offset] == '\n');
	if (t->input[t->offset] == '\r' && t->input[t->offset + 1] == '\n'){
		i++;
		t->offset++;
	}
	t->offset++;
	return i;
}


static int config_tokenizer(server* srv, tokenizer_t* t, int* token_id, buffer* token){
	int tid;
	const char* start;
	int i;

	for (tid = 0; tid == 0 && t->offset < t->size && t->input[t->offset];){
		char c = t->input[t->offset];

		switch (c){
		case '=':
			if (t->in_brace){
				if (t->input[t->offset + 1] == '>'){
					t->offset += 2;
					buffer_append_string_len(token, CONST_STR_LEN("=>"));

					tid = TK_ARRAY_ASSIGN;
				}else{
					log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
						"source: ", t->source, "line: ", t->line,
						"pos: ", t->line_pos,
						"use => for assignments in arrays");
					return -1;
				}
			}else if(t->in_cond){
				if (t->input[t->offset + 1] == '='){
					t->offset += 2;
					buffer_copy_string_len(token, CONST_STR_LEN("=="));
					tid = TK_EQ;
				}else if (t->input[t->offset + 1] == '~'){
					t->offset += 2;
					buffer_copy_string_len(token, CONST_STR_LEN("=~"));
					tid = TK_MATCH;
				}else{
					log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
						"source: ", t->source, "line: ", t->line,
						"pos: ", t->line_pos, "only == and =~ are allowed in the condition");
					return -1;
				}
				t->in_cond = 0;
				t->in_key = 1;
			}else if (t->in_key){
				tid = TK_ASSIGN;
				buffer_copy_string_len(token, t->input + t->offset, 1);
				t->offset++;
				t->line_pos++;
			}else{
				log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
					"source: ", t->source, "line: ", t->line,
					"pos: ", t->line_pos, "unexpected equal-sign: = ");
				return -1;
			}
			break;
		case '!':
			if (t->in_cond){
				if (t->input[t->offset + 1] == '='){
					t->offset += 2;
					buffer_copy_string_len(token, CONST_STR_LEN("!="));
					tid = TK_NE;
				}else if (t->input[t->offset + 1] == '~'){
					t->offset += 2;
					buffer_copy_string_len(token, CONST_STR_LEN("!~"));
					tid = TK_NOMATCH;
				}else{
					log_error_write(srv,__FILE__,__LINE__,"sbsdsds",
						"source: ", t->source, "line: ", t->line,
						"pos: ", t->line_pos, "only != and !~ are in condition");
					return -1;
				}
				t->in_cond = 0;
				t->in_key = 1;
			}else{
				log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
					"source: ", t->source, "line: ", t->line,
					"pos: ", t->line_pos, "unexpected exclamation-marks: !");
				return -1;
			}
			break;
		case '\t':
		case ' ':
			t->offset++;
			t->line_pos++;
			break;
		case '\r':
		case '\n':
			if (t->in_brace == 0){
				int done = 0;
				while (!done && t->offset < t->size){
					switch (t->input[t->offset]){
					case '\r':
					case '\n':
						t->line_pos = 1;
						t->line += config_skip_newline(t);
						break;
					case '#':
						t->line_pos += config_skip_comment(t);
						break;
					case '\t':
					case ' ':
						t->offset++;
						t->line_pos++;
						break;
					default:
						done = 1;
						break;
					}
				}
				t->in_key = 1;
				tid = TK_EOL;
				buffer_copy_string_len(token, CONST_STR_LEN("EOL"));
			}else {
				config_skip_comment(t);
				t->line++;
				t->line_pos = 1;
			}
			break;
		case ',':
			if (t->in_brace > 0){
				tid = TK_COMMA;
				buffer_copy_string_len(token, CONST_STR_LEN("COMMA"));
			}
			t->offset++;
			t->line_pos++;
			break;
		case '"':
			start = t->input + t->offset + 1;
			for (i = 1; t->input[t->offset + i]; i++){
				if (t->input[t->offset + i] == '\\' && t->input[t->offset + i + 1] == '"'){
					buffer_append_string_len(token, start, t->input + t->offset + i - start);
					start = t->input + t->offset + i + 1;
					i++;
					continue;
				}
			
				if (t->input[t->offset + i] == '"'){
					tid = TK_STRING;
					buffer_append_string_len(token, start, t->input + t->offset + i - start);
					break;
				}
			}

			if (t->input[t->offset + i] == '\0'){
				log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
					"source: ", t->source, "line: ", t->line,
					"pos: ", t->line_pos, "missing closing quote");
				return -1;
			}
			t->offset += i + 1;
			t->line_pos += i + 1;
			break;
		case '(':
			t->offset++;
			t->in_brace++;
			tid = TK_LPARAN;
			buffer_copy_string_len(token, CONST_STR_LEN("("));
			break;
		case ')':
			t->offset++;
			t->in_brace--;
			tid = TK_RPARAN;
			buffer_copy_string_len(token, CONST_STR_LEN(")"));
			break;
		case '$':
			t->offset++;
			t->in_cond = 1;
			t->in_key = 0;
			tid = TK_DOLLAR;
			buffer_copy_string_len(token, CONST_STR_LEN("$"));
			break;
		case '+':
			if (t->input[t->offset + 1] == '='){
				t->offset += 2;
				buffer_copy_string_len(token, CONST_STR_LEN("+="));
				tid = TK_APPEND;
			}else{
				t->offset++;
				buffer_copy_string_len(token, CONST_STR_LEN("+"));
				tid = TK_PLUS;
			}
			break;
		case '{':
			t->offset++;
			buffer_copy_string_len(token, CONST_STR_LEN("{"));
			tid = TK_LCURLY;
			break;
		case '}':
			t->offset++;
			buffer_copy_string_len(token, CONST_STR_LEN("}"));
			tid = TK_RCURLY;
			break;
		case '[':
			t->offset++;
			buffer_copy_string_len(token, CONST_STR_LEN("["));
			tid = TK_LBRACKET;
			break;
		case ']':
			t->offset++;
			buffer_copy_string_len(token, CONST_STR_LEN("]"));
			tid = TK_RBRACKET;
			break;
		case '#':
			t->line_pos += config_skip_comment(t);
			break;
		default:
			if (t->in_cond){
				for (i = 0; t->input[t->offset + i] &&
					isalpha(t->input[t->offset + i]); i++);

				if (i && t->input[t->offset + i]){
					buffer_copy_string_len(token, t->input + t->offset, i);
					tid = TK_SRVVARNAME;
					t->offset += i;
					t->line_pos += i;
				}else{
					log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
						"source: ", t->source, "line: ", t->line,
						"pos: ", t->line_pos, "invalid character in condition");
					return -1;
				}
			}else if (isdigit(c)){
				for (i = 0; t->input[t->offset + i] && isdigit(t->input[t->offset + i]); i++);

				if (i && t->input[t->offset + i]){
					tid = TK_INTEGER;
					buffer_copy_string_len(token, t->input + t->offset, i);
					t->offset += i;
					t->line_pos += i;
				}
			}else{
				for (i = 0; t->input[t->offset + i] &&
					(isalnum(t->input[t->offset + i]) || t->input[t->offset + i] == '.' ||
					t->input[t->offset + i] == '_' || t->input[t->offset + i] == '-'); i++);

				if (i && t->input[t->offset + i]){
					buffer_copy_string_len(token, t->input + t->offset, i);

					if (strcmp(token->ptr, "include") == 0){
						tid = TK_INCLUDE;
					}else if (strcmp(token->ptr, "include_shell") == 0){
						tid = TK_INCLUDE_SHELL;
					}else if (strcmp(token->ptr, "global") == 0){
						tid = TK_GLOBAL;
					}else if (strcmp(token->ptr, "else") == 0){
						tid = TK_ELSE;
					}else{
						tid = TK_LKEY;
					}
					t->offset += i;
					t->line_pos += i;
				}else{
					log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
						"source: ", t->source, "line: ", t->line,
						"pos: ", t->line_pos, "invalid character in variable name");
					return -1;
				}
			}
			break;
		}
	}
	if (token_id){
		*token_id = tid;
		return 1;
	}else if (t->offset < t->size){
		fprintf(stderr, "%s.%d: %d,%s\n", __FILE__, __LINE__, tid, token->ptr);
	}
	return 0;
}


static int config_parse(server* srv, config_t* context, tokenizer_t* t){
	buffer *token, *lasttoken;
	void* parser;
	int ret;
	int token_id;

	parser = configparserAlloc(malloc);
	force_assert(parser != NULL);
	token = buffer_init();
	lasttoken = buffer_init();

	while (1 == (ret = config_tokenizer(srv, t, &token_id, token)) && context->ok){
		buffer_copy_buffer(lasttoken, token);
		configparser(parser, token_id, token, context);

		token = buffer_init();
	}
	buffer_free(token);

	if (-1 == ret && context->ok){
	
	}else if (!context->ok){
	
	}
	buffer_free(lasttoken);

	return ret == -1 ? -1 : 0;
}


static int config_parse_file_stream(server* srv, config_t* context, const buffer* filename){
	stream s;
	tokenizer_t t;
	int ret;

	if (0 != stream_open(&s, filename)){
		log_error_write(srv, __FILE__, __LINE__, "sbss",
			"opening configfile ", filename, "failed ", strerror(errno));
		return -1;
	}else{
		tokenizer_init(&t, filename, s.start, s.size);
		ret = config_parse(srv, context, &t);
	}
	stream_close(&s);
	return ret;
}

static int config_insert(server* srv){
	size_t i;
	int ret = 0;
	buffer* stat_cache_string;

	config_values_t cv[] = {
		{ "server.bind",				NULL,	T_CONFIG_STRING,	T_CONFIG_SCOPE_SERVER },/*0*/
		{ "server.errorlog",			NULL,	T_CONFIG_STRING,	T_CONFIG_SCOPE_SERVER },/*1*/
		{ "server.chroot",				NULL,	T_CONFIG_STRING,	T_CONFIG_SCOPE_SERVER },/*3*/
		{ "server.modules",				NULL,	T_CONFIG_ARRAY,		T_CONFIG_SCOPE_SERVER },/*9*/
		{ "server.event-handler",		NULL,	T_CONFIG_STRING,	T_CONFIG_SCOPE_SERVER },/*10*/
		{ "server.stat_cache_engine",	NULL,	T_CONFIG_STRING,	T_CONFIG_SCOPE_SERVER },/*42*/
		{ "server.upload-dirs",			NULL,	T_CONFIG_ARRAY,		T_CONFIG_SCOPE_SERVER },/*45*/
		
		{NULL,							NULL,	T_CONFIG_UNSET,		T_CONFIG_SCOPE_UNSET}
	};

	cv[0].destination = srv->srvconf.bindhost;
	cv[1].destination = srv->srvconf.errorlog_file;
	cv[2].destination = srv->srvconf.changeroot;	
	cv[3].destination = srv->srvconf.modules;
	cv[4].destination = srv->srvconf.event_handler;
	stat_cache_string = buffer_init();
	cv[5].destination = stat_cache_string;

	cv[6].destination = srv->srvconf.upload_tempdirs;

	srv->config_storage = calloc(1, srv->config_context->used * sizeof(*srv->config_storage));
	force_assert(srv->config_storage != NULL);
	force_assert(srv->config_context->used);

	for (i = 0; i < srv->config_context->used; i++){
		data_config const* config = (data_config const*)srv->config_context->data[i];
		
		specific_config* s = calloc(1, sizeof(*s));
		force_assert(s != NULL);

		s->document_root = buffer_init();
		s->mimetypes = array_init();
		s->ssl_pemfile = buffer_init();

		srv->config_storage[i] = s;

		if (0 != (ret = config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION))){
			break;
		}
	}

	data_config const* config = (data_config const*)srv->config_context->data[0];
	config->print((data_unset*)config, 2);

	{
		specific_config* s = srv->config_storage[0];
		s->http_parseopts =
			(srv->srvconf.http_header_strict ? (HTTP_PARSEOPT_HEADER_STRICT) : 0)
			| (srv->srvconf.http_host_strict ? (HTTP_PARSEOPT_HOST_STRICT | HTTP_PARSEOPT_HOST_NORMALIZE) : 0)
			| (srv->srvconf.http_host_normalize ? (HTTP_PARSEOPT_HOST_NORMALIZE) : 0);
	}

	if (buffer_string_is_empty(stat_cache_string)){
		srv->srvconf.stat_cache_engine = STAT_CACHE_ENGINE_SIMPLE;
	}else if (buffer_is_equal_string(stat_cache_string,CONST_STR_LEN("simple"))){
		srv->srvconf.stat_cache_engine = STAT_CACHE_ENGINE_SIMPLE;
#ifdef HAVE_FAM_H
	}else if (buffer_is_equal_string(stat_cache_string, CONST_STR_LEN("fam"))){
		srv->srvconf.stat_cache_engine = STAT_CACHE_ENGINE_FAM;
#endif
	}else if (buffer_is_equal_string(stat_cache_string, CONST_STR_LEN("disable"))){
		srv->srvconf.stat_cache_engine = STAT_CACHE_ENGINE_NONE;
	}else{
		log_error_write(srv, __FILE__, __LINE__, "sb",
			"server.stat_cache_engine can be one of \"disable\", \"simple\","
#ifdef HAVE_FAM_H
			" \"fam\","
#endif
			" but not: ", stat_cache_string);
		return HANDLER_ERROR;
	}
	buffer_free(stat_cache_string);

	{
		int prepend_mod_indexfile = 1;
		int append_mod_dirlisting = 1;
		int append_mod_staticfile = 1;
		int append_mod_authn_file = 1;
		int append_mod_authn_ldap = 1;
		int append_mod_authn_mysql = 1;
		int contains_mod_auth = 0;

		for (i = 0; i < srv->srvconf.modules->used; i++){
			data_string* ds = (data_string*)srv->srvconf.modules->data[i];

			if (buffer_is_equal_string(ds->value, CONST_STR_LEN("mod_indexfile"))){
				prepend_mod_indexfile = 0;
			}

			if (buffer_is_equal_string(ds->value, CONST_STR_LEN("mod_staticfile"))){
				append_mod_staticfile = 0;
			}

			if (buffer_is_equal_string(ds->value, CONST_STR_LEN("mod_dirlisting"))){
				append_mod_dirlisting = 0;
			}

			if (buffer_is_equal_string(ds->value, CONST_STR_LEN("mod_authn_file"))){
				append_mod_authn_file = 0;
			}

			if (buffer_is_equal_string(ds->value, CONST_STR_LEN("mod_authn_ldap"))){
				append_mod_authn_ldap = 0;
			}

			if (buffer_is_equal_string(ds->value, CONST_STR_LEN("mod_authn_mysql"))){
				append_mod_authn_mysql = 0;
			}

			if (buffer_is_equal_string(ds->value, CONST_STR_LEN("mod_auth"))){
				contains_mod_auth = 1;
			}

			if (0 == prepend_mod_indexfile &&
				0 == append_mod_dirlisting &&
				0 == append_mod_staticfile &&
				0 == append_mod_authn_file &&
				0 == append_mod_authn_ldap &&
				0 == append_mod_authn_mysql &&
				1 == contains_mod_auth)
				break;
		}		

		if (prepend_mod_indexfile){
			array* a = array_init();

			data_string* ds = data_string_init();
			buffer_copy_string_len(ds->value, CONST_STR_LEN("mod_indexfile"));
			array_insert_unique(a, (data_unset*)ds);

			for (i = 0; i < srv->srvconf.modules->used; i++){
				data_unset* du = srv->srvconf.modules->data[i];
				array_insert_unique(a, du->copy(du));
			}
			array_free(srv->srvconf.modules);
			srv->srvconf.modules = a;
		}

		if (append_mod_dirlisting){
			data_string* ds = data_string_init();
			buffer_copy_string_len(ds->value, CONST_STR_LEN("mod_dirlisting"));
			array_insert_unique(srv->srvconf.modules, (data_unset*)ds);
		}

		if (append_mod_staticfile){
			data_string* ds = data_string_init();
			buffer_copy_string_len(ds->value, CONST_STR_LEN("mod_staticfile"));
			array_insert_unique(srv->srvconf.modules, (data_unset*)ds);
		}

		if (contains_mod_auth){
			if (append_mod_authn_file){
				data_string* ds = data_string_init();
				buffer_copy_string_len(ds->value, CONST_STR_LEN("mod_authn_file"));
				array_insert_unique(srv->srvconf.modules, (data_unset*)ds);
			}

			if (append_mod_authn_ldap){
			#if defined(HAVE_LDAP_H) && defined(HAVE_LBER_H) && defined(HAVE_LIBDAP) && defined (HAVE_LIBBER)
				config_warn_authn_module(srv, "ldap");
			#endif
			}

			if (append_mod_authn_mysql){
			#if defined(HAVE_MYSQL_H)
				config_warn_authn_module(srv, "mysql");
			#endif
			}
		}
	}
	return ret;
}


int config_read(server* srv, const char* fn){
	config_t context;
	data_config* dc;
	data_integer* dpid;
	data_string* dcwd;
	buffer* filename;
	char* pos;
	int ret;

	config_init(srv, &context);
	context.all_configs = srv->config_context;

	pos = strrchr(fn, '/');
	if (pos)
		buffer_copy_string_len(context.basedir, fn, pos - fn + 1);

	dc = data_config_init();
	buffer_copy_string_len(dc->key, CONST_STR_LEN("global"));

	force_assert(context.all_configs->used == 0);
	dc->context_ndx = context.all_configs->used;
	array_insert_unique(context.all_configs, (data_unset*)dc);
	context.current = dc;

	dpid = data_integer_init();
	dpid->value = getpid();
	buffer_copy_string_len(dpid->key, CONST_STR_LEN("var.PID"));
	array_insert_unique(dc->value, (data_unset*)dpid);

	dcwd = data_string_init();
	buffer_string_prepare_copy(dcwd->value, 1023);
	if (NULL != getcwd(dcwd->value->ptr, dcwd->value->size - 1)){
		buffer_commit(dcwd->value, strlen(dcwd->value->ptr));
		buffer_copy_string_len(dcwd->key, CONST_STR_LEN("var.CWD"));
		array_insert_unique(dc->value, (data_unset*)dcwd);
	}else{
		dcwd->free((data_unset*)dcwd);
	}

	filename = buffer_init_string(fn);
	ret = config_parse_file_stream(srv, &context, filename);
	if (ret != 0)
		return ret;
	if (0 != config_insert(srv))
		return -1;
	return 0;
}

int config_set_defaults(server* srv){
	size_t i;
	struct stat st1, st2;
	specific_config* s = srv->config_storage[0];

	struct evt_map{ fdevent_handler_t et; const char* name; } event_handlers[] = {
#ifdef USE_LINUX_EPOLL
		{ FDEVENT_HANDLER_LINUX_SYSEPOLL, "linux-sysepoll" },
#endif
#ifdef USE_POLL
		{ FDEVENT_HANDLER_POLL, "poll" },
#endif
#ifdef USE_SELECT
		{ FDEVENT_HANDLER_SELECT, "select" },
#endif
#ifdef USE_LIBEV
		{ FDEVENT_HANDLER_LIBEV, "libev" },
#endif
#ifdef USE_SOLARIS_DEVPOLL
		{ FDEVENT_HANDLER_SOLARIS_DEVPOLL, "solaris-devpoll" },
#endif
#ifdef USE_SOLARIS_PORT
		{ FDEVENT_HANDLER_SOLARIS_PORT, "solaris-eventport" },
#endif
#ifdef USE_FREEBSD_KQUEUE
		{ FDEVENT_HANDLER_FREEBSD_KQUEUE, "freebsd-kqueue" },
		{ FDEVENT_HANDLER_FREEBSD_KQUEUE, "kqueue" },
#endif
		{FDEVENT_HANDLER_UNSET, NULL}
	};

	if (!buffer_string_is_empty(srv->srvconf.changeroot)){
		if (-1 == stat(srv->srvconf.changeroot->ptr, &st1)){
			log_error_write(srv, __FILE__, __LINE__, "sb", "server.chroot doesn't exist", srv->srvconf.changeroot);
			return -1;
		}
		if (!S_ISDIR(st1.st_mode)){
			log_error_write(srv, __FILE__, __LINE__, "sb", "server.chroot isn't a directory", srv->srvconf.changeroot);
			return -1;
		}
	}

	if (!srv->srvconf.upload_tempdirs->used){
		data_string* ds = data_string_init();
		const char* tempdir = getenv("TEMPDIR");
		if (tempdir == NULL)	tempdir = "/var/temp";
		buffer_copy_string(ds->value, tempdir);
		array_insert_unique(srv->srvconf.upload_tempdirs, (data_unset*)ds);
	}

	if (srv->srvconf.upload_tempdirs->used){
		buffer* const b = srv->tmp_buf;
		size_t len;
		if (!buffer_string_is_empty(srv->srvconf.changeroot)){
			buffer_copy_buffer(b, srv->srvconf.changeroot);
			buffer_append_slash(b);
		}else{
			buffer_reset(b);
		}

		len = buffer_string_length(b);
		for (i = 0; i < srv->srvconf.upload_tempdirs->used; i++){
			const data_string* const ds = (data_string*)srv->srvconf.upload_tempdirs->data[i];
			buffer_string_set_length(b, len);
			buffer_append_string_buffer(b, ds->value);
			if (-1 == stat(b->ptr, &st1)){
				log_error_write(srv, __FILE__, __LINE__, "sb", "server.upload_tempdirs doesn't exist", b);
				return -1;
			}
			if (!S_ISDIR(st1.st_mode)){
				log_error_write(srv, __FILE__, __LINE__, "sb", "server.upload_tempdirs isn't a directory", b);
				return -1;
			}
		}
	}

	chunkqueue_set_tempdirs_default(srv->srvconf.upload_tempdirs, srv->srvconf.upload_temp_file_size);

	if (buffer_string_is_empty(s->document_root)){
		log_error_write(srv, __FILE__, __LINE__, "s", "a default document-root has to be set");
		return -1;
	}

	buffer_copy_buffer(srv->tmp_buf, s->document_root);

	buffer_to_lower(srv->tmp_buf);

	if (2 == s->force_lowercase_filenames){
		s->force_lowercase_filenames = 0;

		if (stat(srv->tmp_buf->ptr, &st1) == 0){
			int is_lower = 0;
			is_lower = buffer_is_equal(srv->tmp_buf, s->document_root);
			
			buffer_copy_buffer(srv->tmp_buf, s->document_root);

			buffer_to_upper(srv->tmp_buf);
			if (is_lower && buffer_is_equal(srv->tmp_buf, s->document_root)){
				s->force_lowercase_filenames = 0;
			}
			else if (0 == stat(srv->tmp_buf->ptr, &st2)){
				if (st1.st_ino == st2.st_ino){
					s->force_lowercase_filenames = 1;
				}
			}
		}
	}

	if (srv->srvconf.port == 0)
		srv->srvconf.port = s->ssl_enabled ? 443 : 80;
	
	if (buffer_string_is_empty(srv->srvconf.event_handler)){
		srv->event_handler = event_handlers[0].et;

		if (srv->event_handler == FDEVENT_HANDLER_UNSET){
			log_error_write(srv, __FILE__, __LINE__, "s", "sorry,there is no event-handler for this system");
			return -1;
		}
	}else{
		for (i = 0; event_handlers[i].name; i++){
			if (0==strcmp(event_handlers[i].name, srv->srvconf.event_handler->ptr)){
				srv->event_handler = event_handlers[i].et;
				break;
			}
		}
		if (srv->event_handler == FDEVENT_HANDLER_UNSET){
			log_error_write(srv, __FILE__, __LINE__, "s", "the selected event-handler in unknown or not supported");
			return -1;
		}
	}

	if (s->ssl_enabled){
		if (buffer_is_empty(s->ssl_pemfile)){
			log_error_write(srv, __FILE__, __LINE__, "s", "ssl.pemfile has to be set");
			return -1;
		}
#ifndef USE_OPENSSL
		log_error_write(srv, __FILE__, __LINE__, "s", "ssl support missing,recompile with --with-openssl");
		return -1;
#endif
	}
	return 0;
}


int config_parse_file(server* srv, config_t* context, const char* fn){


}


int config_parse_cmd(server* srv, config_t* context, const char* cmd){

}