/*
 * Copyright (c) 2013, Thomas Barker
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * It under the terms of the GNU General Public License as published by
 * The Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * Along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>

#include "common.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

char *
strsep(char *restrict *restrict s, const char *restrict delimiters)
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

char *
strpcpy(char *restrict dst, const char *restrict src)
{
	size_t len = strlen(src) + 1;
	memcpy(dst, src, len);
	return dst + len;
}

wchar_t *
utf8to16(const char *restrict s)
{
	static wchar_t ws[MAX_SERVER_MESSAGE];
	MultiByteToWideChar(CP_UTF8, 0, s, -1, ws, LENGTH(ws));
	return ws;
}

char *
utf16to8(const wchar_t *restrict ws)
{
	static char s[MAX_SERVER_MESSAGE];
	WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, LENGTH(s), NULL, NULL);
	return s;
}
