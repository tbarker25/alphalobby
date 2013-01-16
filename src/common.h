#pragma once

#define MAX_SERVER_MESSAGE 1024
#define MAX_TEXT_MESSAGE_LENGTH 900

#define MAX_NAME_LENGTH 21 //Hardcoded into Uberserver
#define MAX_NAME_LENGTH_NUL 22 //Hardcoded into Uberserver
#define MAX_PASSWORD_LENGTH 24 //Length of the md5hash
#define MAX_TITLE 128 //Also used for mapnames, modnames. Overflow is truncated to fit

#define _STR(x) #x
#define STRINGIFY(x) _STR(x)

char *strsep(char **restrict s, const char * delim);
char *strpcpy(char *dst, const char *src);

wchar_t *utf8to16(const char *str);
char *utf16to8(const wchar_t *wStr);

