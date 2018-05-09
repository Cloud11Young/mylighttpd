#include "marray.h"
#include "mbase.h"
#include <stdio.h>

static struct data_unset* data_config_copy(const data_unset* s){
	data_config* src = (data_config*)s;
	data_config* dc = data_config_init();

	buffer_copy_buffer(dc->key, src->key);
	buffer_copy_buffer(dc->comp_key, src->comp_key);
	array_free(dc->value);
	dc->value = array_init_array(src->value);
	
	return (data_unset*)dc;
}


static void data_config_free(data_unset* p){
	data_config* dc = (data_config*)p;
	
	buffer_free(dc->key);
	buffer_free(dc->comp_key);
	buffer_free(dc->op);
	buffer_free(dc->string);

	vector_config_weak_free(&dc->children);

	array_free(dc->value);

#if defined HAVE_PCRE_H
	if (dc->regex)	pcre_free(dc->regex);
	if (dc->regex_study)	pcre_free(dc->regex_study);
#endif
	free(dc);
}


static void data_config_reset(data_unset* p){
	data_config* dc = (data_config*)p;

	buffer_reset(dc->key);
	buffer_reset(dc->comp_key);
	array_reset(dc->value);
}


static int data_config_insert_dup(data_unset* dst, data_unset* src){
	UNUSED(dst);
	src->free(src);
	return 0;
}


static void data_config_print(data_unset* p, int depth){
	data_config* dc = (data_config*)p;
	array* a = dc->value;
	size_t maxlen;
	size_t i;

	if (dc->context_ndx == 0){
		fprintf(stdout, "config {\n");
	}else{
		if (dc->cond != CONFIG_COND_ELSE){
			fprintf(stdout, "$%s - %s \"%s\" {\n", dc->comp_key->ptr, dc->op->ptr, dc->string->ptr);
		}else{
			fprintf(stdout, "{\n");
		}
		array_print_indent(depth + 1);
		fprintf(stdout, "# block %d\n", dc->context_ndx);
	}

	depth++;
	maxlen = array_get_max_key_length(a);
	for (i = 0; i < dc->value->used; i++){
		data_unset* du = dc->value->data[i];
		size_t len = buffer_string_length(du->key);
		size_t j;

		array_print_indent(depth);
		fprintf(stdout, "%s", du->key->ptr);
		for (j = maxlen - len; j>0; j--){
			fprintf(stdout, " ");
		}
		fprintf(stdout, " = ");
		du->print(du, depth);
		fprintf(stdout, "\n");
	}

	fprintf(stdout, "\n");
	for (i = 0; i < dc->children.used; i++){
		data_config* d = dc->children.data[i];
		
		if (d->prev == NULL){
			fprintf(stdout, "\n");
			array_print_indent(depth);
			d->print((data_unset*)d, depth);
			fprintf(stdout, "\n");
		}
	}

	depth--;
	array_print_indent(depth);
	fprintf(stdout, "}");

	if (0 != dc->context_ndx){
		if (dc->cond != CONFIG_COND_ELSE){
			fprintf(stdout, "# end of $%s %s \"%s\" ", dc->comp_key->ptr, dc->op->ptr, dc->string->ptr);
		}else{
			fprintf(stdout, "# end of else");
		}
	}

	if (dc->next){
		fprintf(stdout, "\n");
		array_print_indent(depth);
		fprintf(stdout, "else ");
		dc->next->print((data_unset*)dc->next, depth);
	}
}


data_config* data_config_init(){
	data_config* dc = calloc(1, sizeof(*dc));
	force_assert(dc != NULL);

	dc->type = TYPE_CONFIG;
	dc->key=buffer_init();
	dc->comp_key = buffer_init();
	dc->op = buffer_init();
	dc->string = buffer_init();
	vector_config_weak_init(&dc->children);
	dc->value = array_init();

	dc->copy = data_config_copy;
	dc->free = data_config_free;
	dc->reset = data_config_reset;
	dc->insert_dup = data_config_insert_dup;
	dc->print = data_config_print;
	return dc;
}
