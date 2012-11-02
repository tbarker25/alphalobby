#pragma once

#define MAX_SERVER_MESSAGE 1024
#define MAX_TEXT_MESSAGE_LENGTH 900

#define MAX_NAME_LENGTH 21 //Hardcoded into Uberserver
#define MAX_NAME_LENGTH_NUL 22 //Hardcoded into Uberserver
#define MAX_PASSWORD_LENGTH 24 //Length of the md5hash
#define MAX_TITLE 128 //Also used for mapnames, modnames. Overflow is truncated to fit
#define NUM_TEAMS 16
#define NUM_ALLIANCES 16

#define _STR(x) #x
#define STRINGIFY(x) _STR(x)

#define FOR_EACH(i, L)\
	for (typeof(*L) *i = L; i - L < LENGTH(L); ++i)

char *strsplit(char **restrict s, const char * delim);
char *strpcpy(char *dst, const char *src);

#ifdef _WCHAR_T
wchar_t *utf8to16(const char *str);
char *utf16to8(const wchar_t *wStr);
#endif

#ifndef NDEBUG
	#define STARTCLOCK()\
		long _startTime = GetTickCount();\
		printf("entering %s %s %d\n", __FUNCTION__, __FILE__, __LINE__)
	#define ENDCLOCK() {\
		long _lastTime = _startTime;\
		printf("\ntotal time:%ld\nIn %s %s %d\n\n", (_startTime=GetTickCount()) - _lastTime, __FUNCTION__, __FILE__, __LINE__);\
	}
#else 
	#define STARTCLOCK()
	#define ENDCLOCK()
#endif

