#include <windows.h>
#include <windowsx.h>
#include <oleacc.h>
#include <Commctrl.h>
#include <stdio.h>
#include <richedit.h>
#include <Commctrl.h>
#include <inttypes.h>

#include "common.h"
#include "countryCodes.h"
#include "layoutmetrics.h"
#include "settings.h"
#include "wincommon.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

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

wchar_t *utf8to16(const char *str)
{
	static wchar_t wStr[MAX_SERVER_MESSAGE];
	MultiByteToWideChar(CP_UTF8, 0, str, -1, wStr, LENGTH(wStr));
	return wStr;
}

char *utf16to8(const wchar_t *wStr)
{
	static char str[MAX_SERVER_MESSAGE];
	WideCharToMultiByte(CP_UTF8, 0, wStr, -1, str, LENGTH(str), NULL, NULL);
	return str;
}
