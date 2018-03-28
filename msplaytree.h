#ifndef SPLAY_TREE_H_
#define SPLAY_TREE_H_

typedef struct tree_node{
	struct tree_node *left, *right;
	int key;
	int size;
	void* data;
}splay_tree;

#endif