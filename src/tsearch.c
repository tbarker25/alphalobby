/*
 * original code from MUSL (bsd licensed)
 * modified by Thomas Barker 2013-07-21
 */

#include <stdlib.h>
#include <search.h>

#include "tsearch.h"

/*
avl tree implementation using recursive functions
the height of an n node tree is less than 1.44*log2(n+2)-1
(so the max recursion depth in case of a tree with 2^32 nodes is 45)
*/

struct node {
	struct node *left;
	struct node *right;
	int height;
	char key[];
};

static int
delta(struct node *n)
{
	return (n->left ? n->left->height:0) - (n->right ? n->right->height:0);
}

static void
updateheight(struct node *restrict n)
{
	struct node *restrict l = n->left;
	struct node *restrict r = n->right;

	n->height = 0;
	if (l && l->height > n->height)
		n->height = l->height;
	if (r && r->height > n->height)
		n->height = r->height;
	++n->height;
}

static struct node *
rotl(struct node *restrict n)
{
	struct node *restrict r = n->right;

	n->right = r->left;
	r->left = n;
	updateheight(n);
	updateheight(r);
	return r;
}

static struct node *
rotr(struct node *restrict n)
{
	struct node *restrict l = n->left;

	n->left = l->right;
	l->right = n;
	updateheight(n);
	updateheight(l);
	return l;
}

static struct node *
balance(struct node *n)
{
	int d = delta(n);

	if (d < -1) {
		if (delta(n->right) > 0)
			n->right = rotr(n->right);
		return rotl(n);
	} else if (d > 1) {
		if (delta(n->left) < 0)
			n->left = rotl(n->left);
		return rotr(n);
	}
	updateheight(n);
	return n;
}

static struct node *
insert(const void *restrict k, struct node **restrict n, int (*cmp)(const void *, const void *), size_t size)
{
	struct node *r = *n;
	int c;

	if (!r) {
		*n = r = calloc(1, sizeof **n + size);
		r->height = 1;
		return r;
	}
	c = cmp(k, r->key);
	if (c == 0)
		return r;
	if (c < 0)
		r = insert(k, &r->left, cmp, size);
	else
		r = insert(k, &r->right, cmp, size);
	*n = balance(*n);
	return r;
}

static struct node *
movr(struct node *restrict n, struct node *restrict r)
{
	if (!n)
		return r;
	n->right = movr(n->right, r);
	return balance(n);
}

static struct node *
remove(struct node **restrict n, const void *restrict k, int (*cmp)(const void *, const void *), struct node *restrict parent)
{
	int c;

	if (!*n)
		return 0;
	c = cmp(k, (*n)->key);
	if (c == 0) {
		struct node *r = *n;
		*n = movr(r->left, r->right);
		free(r);
		return parent;
	}
	if (c < 0)
		parent = remove(&(*n)->left, k, cmp, *n);
	else
		parent = remove(&(*n)->right, k, cmp, *n);
	if (parent)
		*n = balance(*n);
	return parent;
}

void
_tdelete(const void *restrict key, void **restrict rootp, int(*compar)(const void *, const void *))
{
	remove((void*)rootp, key, compar, *rootp);
}

void *
_tfind(const void *restrict key, void *restrict rootp, int(*cmp)(const void *, const void *))
{
	struct node *n = rootp;

	while (n) {
		int c = cmp(key, n->key);
		if (c == 0)
			return n->key;
		n = c < 0 ? n->left : n->right;
	}
	return NULL;
}

void *
_tinsert(const void *restrict key, void **restrict rootp, int (*compar)(const void *, const void *), size_t size)
{
	return insert(key, (struct node **)rootp, compar, size)->key;
}

void
_twalk(const void *root, void (*action)(const void *))
{
	const struct node *n = root;
	const struct node *parents[n->height];
	const struct node **top = parents;

	while (1) {
		if (n) {
			*(top++) = n;
			n = n->left;

		} else if (top != parents) {
			n = *--top;
			action(n->key);
			n = n->right;

		} else {
			return;
		}
	}
}

void
_tdestroy(void **rootp)
{
	struct node *n = *rootp;
	struct node *parents[n->height];
	struct node **top = parents;

	*rootp = NULL;
	while (1) {
		if (n) {
			*(top++) = n;
			n = n->left;

		} else if (top != parents) {
			struct node *tmp = *--top;
			n = tmp->right;
			free(tmp);

		} else {
			return;
		}
	}
}
