#ifndef SPLAY_TREE_H_
#define SPLAY_TREE_H_

typedef struct tree_node{
	struct tree_node *left, *right;
	int key;
	int size;
	void* data;
}splay_tree;

splay_tree* splaytree_insert(splay_tree* t, int key, void* data);
splay_tree* splaytree_splay(splay_tree* t, int key);
splay_tree* splaytree_delete(splay_tree* t, int key);
splay_tree* splaytree_size(splay_tree* t);

#define splaytree_size(x) (((x) == NULL) ? 0 : (x->size))

#endif