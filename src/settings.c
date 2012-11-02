#include <windows.h>
#include <windowsx.h>
#include <oleacc.h>
#include <Commctrl.h>
#include "wincommon.h"


#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <Shlobj.h>

#include "common.h"
#include "chat.h"
#include "chat_window.h"
#include "client_message.h"
#include "settings.h"
#include "data.h"

#define CONFIG_PATH (GetDataDir(L"alphalobby.conf"))

typedef struct KeyPair{
	char *key, *val;
	bool isInt;
}KeyPair;

typeof(gSettings) gSettings;
wchar_t gDataDir[MAX_PATH];

static const KeyPair defaultSettings[] = {
	{"spring_path", "spring.exe"},
	{"flags", (void *)0x17, 1},
	{"autojoin", "main"},
	{"selected_packages", NULL},
};

static KeyPair getLine(FILE *fd)
{
	static char buff[1024];
	char *delim=NULL, *end;
	if (!fgets(buff, sizeof(buff), fd)
			|| !(delim = strchr(buff, '='))
			|| !(end = strchr(delim, '\n')))
		return delim ? getLine(fd) : (KeyPair){};
	assert(end - buff < sizeof(buff));
	*end = '\0'; *delim = '\0';
	return (KeyPair){buff, delim+1};
}

char *LoadSetting(const char *key)
{
	static __thread char val[1024];
	FILE *fd = _wfopen(CONFIG_PATH, L"r");
	if (fd) {
		for (KeyPair s; (s = getLine(fd)).key;) {
			if (!strcmp(key, s.key)) {
				strcpy(val, s.val);
				fclose(fd);
				return val;
			}
		}
		fclose(fd);
	}
	return NULL;
}

int LoadSettingInt(const char *key)
{
	return atoi(LoadSetting(key) ?: "0");
}

void SaveSettingInt(const char *key, int val)
{
	char buff[128];
	sprintf(buff, "%d", val);
	SaveSetting(key, buff);
}

void OpenDefaultChannels(void)
{
	if (!gSettings.autojoin)
		return;
	char buff[strlen(gSettings.autojoin) + 1];
	strcpy(buff, gSettings.autojoin);
	char *s, *s2 = buff;
	
	while ((s=strsplit(&s2, ";"))) {
		if (*s == '*')
			ChatWindow_SetActiveTab(GetServerChat());
		else
			JoinChannel(s, 0);
	}
}

void ResetSettings(void)
{
	_wremove(CONFIG_PATH);
}

void InitSettings(void)
{
	SHGetFolderPath(NULL, CSIDL_FLAG_CREATE | CSIDL_PERSONAL, NULL, 0, gDataDir);
	wcscat(gDataDir, L"\\My Games\\Spring\\");
	SHCreateDirectoryEx(NULL, gDataDir, NULL);

	FILE *fd = _wfopen(GetDataDir(L"aliases.conf"), L"r");
	if (fd) {
		for (KeyPair s; (s = getLine(fd)).key;)
			NewUser(atoi(s.key), s.val);
		fclose(fd);
	}
	
	for(int i=0; i<lengthof(defaultSettings); ++i)
		((char **)&gSettings)[i] = defaultSettings[i].isInt ? defaultSettings[i].val
		                         : defaultSettings[i].val ? strdup(defaultSettings[i].val)
								 : NULL;
	
	fd = _wfopen(CONFIG_PATH, L"r");
	if (!fd)
		return;
	for (KeyPair s; (s = getLine(fd)).key;)
		for(int i=0; i<lengthof(defaultSettings); ++i)
			if (!strcmp(defaultSettings[i].key, s.key)) {
				if (defaultSettings[i].isInt)
					((intptr_t *)&gSettings)[i] = atoi(s.val);
				else {
					free(((char **)&gSettings)[i]);
					((char **)&gSettings)[i] = strdup(s.val);
				}
			}
	fclose(fd);
}

void SaveAliases(void)
{
	FILE *aliasFile = _wfopen(GetDataDir(L"aliases.conf"), L"w");
	if (!aliasFile)
		return;
	for (const User *u; (u = GetNextUser());)
		fprintf(aliasFile, "%u=%s\n", u->id, u->alias);
	fclose(aliasFile);
}

// #pragma GCC diagnostic ignored "-Wformat"
void SaveSetting(const char *key, const char *val)
{
	printf("saving %s=%s\n", key, val);
	wchar_t tmpConfigName[MAX_PATH];
	GetTempPath(MAX_PATH, tmpConfigName);
	GetTempFileName(tmpConfigName, NULL, 0, tmpConfigName);

	FILE *tmpConfig = _wfopen(tmpConfigName, L"w");
	assert(tmpConfig);
	if (!tmpConfig)
		return;
	
	const wchar_t *configPath = GetDataDir(L"alphalobby.conf");
	
	FILE *oldConfig = _wfopen(configPath, L"r");
	if (oldConfig) {
		for (KeyPair s; (s = getLine(oldConfig)).key;) {
			if (key && !strcmp(key, s.key))
				goto skipKey;
			for(int i=0; i<lengthof(defaultSettings); ++i)
				if (!strcmp(defaultSettings[i].key, s.key))
					goto skipKey;
			fprintf(tmpConfig, "%s=%s\n", s.key, s.val);
			skipKey:;
		}					
		fclose(oldConfig);
	}
	
	if (key && *val)
		fprintf(tmpConfig, "%s=%s\n", key, val);

	for (int i=0; i<lengthof(defaultSettings); ++i) {
		if (defaultSettings[i].isInt)
			fprintf(tmpConfig, "%s=%d\n", defaultSettings[i].key, ((int *)&gSettings)[i]);
		else if (((void **)&gSettings)[i])
			fprintf(tmpConfig, "%s=%s\n", defaultSettings[i].key, ((char **)&gSettings)[i]);
	}
	
	fclose(tmpConfig);

	#ifdef NDEBUG
		MoveFileEx(tmpConfigName, configPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
	#else
	if (!MoveFileEx(tmpConfigName, configPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
		printf("err = %ld\n", GetLastError());
		assert(0);
	}
	#endif
}

wchar_t * GetDataDir(const wchar_t *file)
{
	static __thread wchar_t buff[MAX_PATH];
	wsprintf(buff, L"%s%s", gDataDir, file);
	return buff;
}
