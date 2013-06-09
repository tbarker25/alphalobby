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

#ifndef COMMON_H
#define COMMON_H

#define MAX_SERVER_MESSAGE 1024
#define MAX_TEXT_MESSAGE_LENGTH 900

#define MAX_NAME_LENGTH 21 //Hardcoded into Uberserver
#define MAX_NAME_LENGTH_NUL 22
#define MAX_PASSWORD_LENGTH 24 //Length of the md5hash

#define _STR(x) #x
#define STRINGIFY(x) _STR(x)

char *strsep(char *restrict *restrict s, const char *restrict delim);
char *strpcpy(char *restrict dst, const char *restrict src);

wchar_t *utf8to16(const char *restrict str);
char *utf16to8(const wchar_t *restrict wStr);

#endif /* end of include guard: COMMON_H */
