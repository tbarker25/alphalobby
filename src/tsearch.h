#ifndef _SEARCH_H
#define _SEARCH_H

void _tdelete(const void *key, void **rootp, int(*)(const void *, const void *));
__attribute__((pure))
void *_tfind(const void *key, void *root, int(*)(const void *, const void *));
void *_tinsert(const void *key, void **rootp, int (*)(const void *, const void *), size_t);
void _twalk(const void *root, void (*)(const void *));
void _tdestroy(void **rootp);

#endif
