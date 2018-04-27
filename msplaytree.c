#include "msplaytree.h"
#include <stdlib.h>
#include <assert.h>

#define compare(x,y) ((x)-(y))
#define node_size splaytree_size

splay_tree* splaytree_insert(splay_tree* t, int key, void* data){
	splay_tree* new;
	if (t != NULL){
		t = splaytree_splay(t, key);
		if (compare(t->key, key) == 0)
			return t;
	}

	new = (splay_tree*)malloc(sizeof(splay_tree));
	assert(new != NULL);

	if (t == NULL){
		new->left = new->right = NULL;
	}else if (compare(t->key, key) > 0){
		new->right = t;
		new->left = t->left;
		t->left = NULL;
		t->size = node_size(t->right) + 1;
	}else{
		new->left = t;
		new->right = t->right;
		t->right = NULL;
		t->size = node_size(t->left) + 1;
	}

	new->key = key;
	new->data = data;
	new->size = 1 + node_size(new->left) + node_size(new->right);
	return new;
}


splay_tree* splaytree_delete(splay_tree* t, int key){
	splay_tree* x;
	size_t tsize;
	if (t == NULL)	return t;

	t = splaytree_splay(t, key);
	tsize = t->size;
	if (compare(t->key, key) == 0){		//found it
		if (t->left == NULL){
			x = t->right;
		}else{
			x = splaytree_splay(t->left, key);
			x->right = t->right;
		}
		free(t);
		if (x){
			x->size = tsize - 1;
		}
		return x;
	}else{
		return t;
	}
}


splay_tree* splaytree_splay(splay_tree* t, int key){
	splay_tree *l, *r, N, *y;
	size_t l_size, r_size;
	int comp;

	if (t == NULL)	return t;
	N.left = N.right = NULL;
	r = l = &N;
	l_size = r_size = 0;

	for (;;){
		comp = compare(key, t->key);

		if (comp < 0){
			if (t->left == NULL)	break;
			if (key < t->left->key){
				y = t->left;
				t->left = y->right;
				y->right = t;
				t->size = node_size(t->left) + node_size(t->right) + 1;
				t = y;
				if (t->left == NULL) break;
			}
			r->left = t;
			r = t;			
			t = t->left;
			r_size += node_size(r->right) + 1;
		}else if (comp > 0){
			if (t->right == NULL)	break;
			if (key > t->right->key){
				y = t->right;
				t->right = y->left;
				y->left = t;
				t->size = node_size(t->left) + node_size(t->right) + 1;
				t = y;
				if (t->right == NULL)	break;
			}
			l->right = t;
			l = t;
			t = t->right;
			l_size += node_size(l->left) + 1;
		}else{
			break;
		}
	}

	l_size += node_size(t->left);
	r_size += node_size(t->right);
	t->size = l_size + r_size + 1;

	for (y = N.right; y != NULL; y = y->right){
		y->size = l_size;
		l_size -= node_size(y->left) + 1;
	}

	for (y = N.left; y != NULL; y = y->left){
		y->size = r_size;
		r_size -= node_size(y->right) + 1;
	}

	l->right = t->left;
	r->left = t->right;
	t->left = N.right;
	t->right = N.left;
	
	return t;
}


// splay_tree* splaytree_size(splay_tree* t){

//}