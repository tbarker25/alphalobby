#ifndef _SEARCH_H
#define _SEARCH_H

void _tdelete(const void *__restrict, void **__restrict, int(*)(const void *, const void *));
void *_tfind(const void *, void *, int(*)(const void *, const void *));
void *_tinsert(const void *, void **, int (*)(const void *, const void *), size_t);
void _twalk(const void *, void (*)(const void *, VISIT, int));
void _tdestroy(void *root);

#endif
