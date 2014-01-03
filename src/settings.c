
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

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <Shlobj.h>

#include "battle.h"
#include "chatbox.h"
#include "chattab.h"
#include "common.h"
#include "mybattle.h"
#include "settings.h"
#include "tasserver.h"
#include "user.h"

#define CONFIG_PATH (Settings_get_data_dir(L"alphalobby.conf"))
#define LENGTH(x) (sizeof x / sizeof *x)

typedef struct KeyPair{
	const char *key, *val;
	char is_int;
}KeyPair;

static KeyPair get_line(FILE *fd);

struct Settings g_settings;
wchar_t g_data_dir[MAX_PATH];

static const KeyPair DEFAULT_SETTINGS[] = {
	{"spring_path", "spring.exe", 0},
	{"flags", (void *)0x17, 1},
	{"autojoin", "main", 0},
	{"selected_packages", NULL, 0},
};

static KeyPair
get_line(FILE *fd)
{
	static char buf[1024];
	char *delim=NULL, *end;
	if (!fgets(buf, sizeof buf, fd)
			|| !(delim = strchr(buf, '='))
			|| !(end = strchr(delim, '\n')))
		return delim ? get_line(fd) : (KeyPair){0};
	assert(end - buf < (ssize_t)sizeof buf);
	*end = '\0'; *delim = '\0';
	return (KeyPair){buf, delim+1, 0};
}

bool
Settings_load_str2(char *restrict buf, const char *restrict key)
{
	FILE *fd = _wfopen(CONFIG_PATH, L"r");
	if (!fd)
		return 1;

	for (KeyPair s; (s = get_line(fd)).key;) {
		if (strcmp(key, s.key))
			continue;

		strcpy(buf, s.val);
		fclose(fd);
		return 0;
	}
	fclose(fd);
	return 1;
}

char *
Settings_load_str(const char *key)
{
	static __thread char buf[1024];
    if (!Settings_load_str2(buf, key))
        buf[0] = '\0';
    return buf;
}

int
Settings_load_int(const char *key)
{
	return atoi(Settings_load_str(key) ?: "0");
}

void
Settings_save_int(const char *key, int val)
{
	char buf[128];
	sprintf(buf, "%d", val);
	Settings_save_str(key, buf);
}

void
Settings_open_default_channels(void)
{
	char buf[strlen(g_settings.autojoin) + 1];

	if (!g_settings.autojoin)
		return;
	strcpy(buf, g_settings.autojoin);
	/* char *s, *s2 = buf; */

	/* while ((s=strsep(&s2, ";"))) { */
		/* if (*s == '*') */
			/* ChatWindow_set_active_tab(Chat_get_server_window()); */
		/* else {} */
			/* Chat_join_channel(s, 0); */
	/* } */
}

void
Settings_reset(void)
{
	_wremove(CONFIG_PATH);
}

void
Settings_init(void)
{
	FILE *fd;

	SHGetFolderPath(NULL, CSIDL_FLAG_CREATE | CSIDL_PERSONAL, NULL, 0, g_data_dir);
	wcscat(g_data_dir, L"\\My Games\\Spring\\");
	SHCreateDirectoryEx(NULL, g_data_dir, NULL);

	for(size_t i=0; i<LENGTH(DEFAULT_SETTINGS); ++i)
		((char **)&g_settings)[i] = DEFAULT_SETTINGS[i].is_int ? (void *)DEFAULT_SETTINGS[i].val
		                         : DEFAULT_SETTINGS[i].val ? _strdup(DEFAULT_SETTINGS[i].val)
								 : NULL;

	fd = _wfopen(CONFIG_PATH, L"r");
	if (!fd)
		return;
	for (KeyPair s; (s = get_line(fd)).key;)
		for(size_t i=0; i<LENGTH(DEFAULT_SETTINGS); ++i)
			if (!strcmp(DEFAULT_SETTINGS[i].key, s.key)) {
				if (DEFAULT_SETTINGS[i].is_int)
					((intptr_t *)&g_settings)[i] = atoi(s.val);
				else {
					free(((char **)&g_settings)[i]);
					((char **)&g_settings)[i] = _strdup(s.val);
				}
			}
	fclose(fd);
}

void
Settings_save_str(const char *key, const char *val)
{
	wchar_t tmp_config_name[MAX_PATH];
	FILE *tmp_config;
	const wchar_t *config_path;
	FILE *old_config;

	printf("saving %s=%s\n", key, val);
	GetTempPath(MAX_PATH, tmp_config_name);
	GetTempFileName(tmp_config_name, NULL, 0, tmp_config_name);

	tmp_config = _wfopen(tmp_config_name, L"w");
	assert(tmp_config);
	if (!tmp_config)
		return;

	config_path = Settings_get_data_dir(L"alphalobby.conf");

	old_config = _wfopen(config_path, L"r");
	if (old_config) {
		for (KeyPair s; (s = get_line(old_config)).key;) {
			if (key && !strcmp(key, s.key))
				goto skip_key;
			for(size_t i=0; i<LENGTH(DEFAULT_SETTINGS); ++i)
				if (!strcmp(DEFAULT_SETTINGS[i].key, s.key))
					goto skip_key;
			fprintf(tmp_config, "%s=%s\n", s.key, s.val);
			skip_key:;
		}
		fclose(old_config);
	}

	if (key && *val)
		fprintf(tmp_config, "%s=%s\n", key, val);

	for (size_t i=0; i<LENGTH(DEFAULT_SETTINGS); ++i) {
		if (DEFAULT_SETTINGS[i].is_int)
			fprintf(tmp_config, "%s=%d\n", DEFAULT_SETTINGS[i].key, ((int *)&g_settings)[i]);
		else if (((void **)&g_settings)[i])
			fprintf(tmp_config, "%s=%s\n", DEFAULT_SETTINGS[i].key, ((char **)&g_settings)[i]);
	}

	fclose(tmp_config);

	#ifdef NDEBUG
		MoveFileEx(tmp_config_name, config_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
	#else
	if (!MoveFileEx(tmp_config_name, config_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
		printf("err = %ld\n", GetLastError());
		assert(0);
	}
	#endif
}

wchar_t *
Settings_get_data_dir(const wchar_t *file)
{
	static __thread wchar_t buf[MAX_PATH];
	assert(*g_data_dir);
	wsprintf(buf, L"%s%s", g_data_dir, file);
	return buf;
}
