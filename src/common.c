#include "common.h"
#include <string.h>

#include <stdio.h>

char *strsplit(char **restrict s, const char *restrict delimiters)
{
	*s += strspn(*s, delimiters);
	if (!**s)
		return NULL;
	char *ret = *s;
	*s += strcspn(*s, delimiters);
	if (**s)
		*(*s)++ = '\0';
	return ret;
}

char *strpcpy(char *dst, const char *src)
{
	size_t len = strlen(src) + 1;
	memcpy(dst, src, len);
	return dst + len;
}
