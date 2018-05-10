#include "mconfigfile.h"

#include <stdio.h>
#include <errno.h>
#include "mbase.h"
#include "mstream.h"
#include "configparser.h"

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

}


int config_parse_file(server* srv, config_t* context, const char* fn){


}


int config_parse_cmd(server* srv, config_t* context, const char* cmd){

}