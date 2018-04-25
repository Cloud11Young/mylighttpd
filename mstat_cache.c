#include <stdlib.h>
#include "mbase.h"
#include "mstat_cache.h"
#include <errno.h>

#ifdef HAVE_FAM_H
typedef struct fam_dir_entry{
	FAMRequest* req;
	buffer* name;
	int version;
}fam_dir_entry;
#endif


static stat_cache_entry* stat_cache_entry_init(){
	stat_cache_entry* sce = calloc(1, sizeof(*sce));
	force_assert(sce != NULL);

	sce->name = buffer_init();
	sce->etag = buffer_init();
	sce->content_type = buffer_init();
	return sce;
}


static void stat_cache_entry_free(void* data){
	stat_cache_entry* sce = (stat_cache_entry*)data;
	if (!sce)	return;

	buffer_free(sce->name);
	buffer_free(sce->etag);
	buffer_free(sce->content_type);
	free(sce);
}


stat_cache* stat_cache_init(){
	stat_cache* sc = calloc(1, sizeof(*sc));
	force_assert(NULL != sc);

	sc->dir_name = buffer_init();
	sc->hash_key = buffer_init();
	return sc;
}

void stat_cache_free(stat_cache* fc){
	force_assert(NULL != fc);
	while (fc->files){
		size_t osize;
		splay_tree* node = fc->files;
		osize = fc->files->size;

		stat_cache_entry_free(node->data);
		fc->files = splaytree_delete(fc->files, node->key);
		
		force_assert(osize - 1 == fc->files->size);
	}

	buffer_free(fc->dir_name);
	buffer_free(fc->hash_key);
	
	free(fc);
}

handler_t stat_cache_get_entry(server* srv, connection* con, buffer* name, stat_cache_entry** ret_sce){
#ifdef HAVE_FAM_H
	fam_dir_entry* fam_dir=NULL;
	int dir_ndx = -1;
#endif
	stat_cache_entry* sce = NULL;
	stat_cache* sc;
	struct stat st;
	struct stat lst;
	int fd;
	int file_ndx;


	*ret_sce = NULL;
	sc = srv->stat_cache;

	buffer_copy_buffer(sc->hash_key, name);
	buffer_append_int(sc->hash_key, con->conf.follow_symlink);

	file_ndx = hashme(sc->hash_key);

	sc->files = splaytree_splay(sc->files, file_ndx);
	
	if (sc->files && sc->files->key == file_ndx){
		
		sce = sc->files->data;

		if (buffer_is_equal(sce->name, name)){
			if (srv->srvconf.stat_cache_engine == STAT_CACHE_ENGINE_SIMPLE){
				if (srv->cur_ts == sce->stat_ts && con->conf.follow_symlink){
					*ret_sce = sce;
					return HANDLER_GO_ON;
				}
			}
		}else{
			sce = NULL;
		}
	}

#ifdef HAVE_FAM_H
	if (srv->srvconf.stat_cache_engine == STAT_CACHE_ENGINE_FAM){
		if (0 != buffer_copy_dirname(sc->dir_name, name)){
			log_error_write(srv, __FILE__, __LINE__, "sb",
				"not '/' found in filename", name);
			return HANDLER_ERROR;
		}

		buffer_copy_buffer(sc->hash_key, sc->dir_name);
		buffer_append_int(sc->hash_key, con->conf.follow_symlink);

		dir_ndx = hashme(sc->hash_key);
		sc->dirs = splaytree_splay(sc->dirs, dir_ndx);

		if (sc->dirs && sc->dirs->key == dir_ndx){
			fam_dir = sc->dirs->data;
			if (buffer_is_equal(fam_dir->name, sc->dir_name)){
				if (sce != NULL && fam_dir->version == sce->dir_version){
					*ret_sce = sce;
					return HANDLER_GO_ON;
				}
			}
			else{
				sce = NULL;
			}
		}
	}
#endif

	if (-1 == stat(name->ptr, &st)){
		return HANDLER_GO_ON;
	}

	if (S_ISREG(st.st_mode)){
		if (name->ptr[buffer_string_length(name) - 1] == '/'){
			errno = ENOTDIR;
			return HANDLER_ERROR;
		}
		if (-1 == (fd = open(name->ptr, O_RDONLY))){
			return HANDLER_ERROR;
		}
		close(fd);
	}

	if (NULL == sce){
		sce = stat_cache_entry_init();
		buffer_copy_buffer(sce->name, name);

		if (sc->files && sc->files->key == file_ndx){
			stat_cache_entry_free(sc->files->data);
			sc->files->data = sce;
		}else{
			size_t osize;
			osize = splaytree_size(sc->files);
			sc->files = splaytree_insert(sc->files, file_ndx, sce);
			force_assert(osize + 1 == splaytree_size(sc->files));
		}

		force_assert(NULL != sc->files);
		force_assert(sce == sc->files->data);
	}

	sce->st = st;
	sce->stat_ts = srv->cur_ts;

#ifdef HAVE_LSTAT
	sce->is_symlink = 0;
	if (!con->conf.follow_symlink){
		if (stat_cache_lstat(srv, name, &lst) == 0){
			sce->is_symlink = 1;
		}else if (buffer_string_length(name) > 1){
			buffer* dname;
			char* s_cur;
			
			dname = buffer_init();
			buffer_copy_buffer(dname, name);

			while ((s_cur = strrchr(dname->ptr, '/'))){
				buffer_string_set_length(dname, s_cur - dname->ptr);
				if (dname->ptr == s_cur){
					log_error_write(srv, __FILE__, __LINE__, "s", "reached /");
					break;
				}

				if (stat_cache_lstat(srv, dname, &lst) == 0){
					sce->is_symlink = 1;
					break;
				}
			}
			buffer_free(dname);
		}
	}
#endif

	if (S_ISREG(st.st_mode)){
		buffer_reset(sce->content_type);
#if defined(HAVE_XATTR) || defined(HAVE_EXTATTR)
		if (con->conf.use_xattr){
			stat_cache_attr_get(sce->content_type, name, srv->srvconf.xattr_name->ptr);
		}
#endif
		if (buffer_string_is_empty(sce->content_type)){
			size_t namelen = buffer_string_length(name);
			size_t k;
			for (k = 0; k < con->conf.mimetypes->used; k++){
				data_string* ds = (data_string*)con->conf.mimetypes->data[k];
				buffer* type = ds->key;
				size_t typelen = buffer_string_length(ds->key);

				if (buffer_is_empty(type))	continue;

				if (namelen < typelen)	continue;

				if (0 == strncasecmp(name->ptr + namelen - typelen, type->ptr, typelen)){
					buffer_copy_buffer(sce->content_type, ds->value);
					break;
				}
			}
		}

		etag_create(sce->etag, &(sce->st), con->etag_flags);
	}else if (S_ISDIR(st.st_mode)){
		etag_create(sce->etag, &(sce->st), con->etag_flags);
	}

#ifdef HAVE_FAM_H
	if (srv->srvconf.stat_cache_engine == STAT_CACHE_ENGINE_FAM){
		if (fam_dir == NULL){
			fam_dir = fam_dir_entry_init();

			buffer_copy_buffer(fam_dir->name, sce->name);

			fam_dir->version = 1;
			fam_dir->req = calloc(1, sizeof(FAMRequest));
			force_assert(NULL != fam_dir->req);

			if (0 != FAMMonitorDirectory(&sc->fam, fam_dir->name->ptr, fam_dir.req, fam_dir)){
				log_error_write(srv, __FILE__, __LINE__, "sbsbs",
					"monitor dir failed:",
					fam_dir->name,
					"file:", name,
					FamErrlist[FAMErrno]);
				fam_dir_entry_free(sc->fam, fam_dir);
				fam_dir = NULL;
			}else{
				size_t osize;
				osize = splaytree_size(sc->dirs);

				if (sc->dirs && sc->dirs->key == dir_ndx){
					fam_dir_entry_free(sc->fam, sc->dirs->data);
					sc->dirs->data = fam_dir;
				}else{
					sc->dirs = splaytree_insert(sc->dirs, dir_ndx, fam_dir);
					force_assert(osize + 1 == splaytree_size(sc->dirs));
				}

				force_assert(sc->dirs);
				force_assert(sc->dirs->data == fam_dir);
			}
		}

		if (fam_dir){
			sce->dir_version = fam_dir->version;
		}
	}
#endif
	*ret_sce = sce;
	return HANDLER_GO_ON;
}

handler_t stat_cache_handle_fdevent(server* srv, void* fce, int revent){

}

int stat_cache_open_rdonly_fstat(server* srv, buffer* name, struct stat* st){

}

int stat_cache_trigger_cleanup(server* srv){

}