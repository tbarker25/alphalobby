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

#ifndef SETTINGS_H
#define SETTINGS_H

#define SETTING_TIMESTAMP_OFFSET (3 + 1)
#define SETTING_HOST_BALANCE_OFFSET (3 + 2)
// #define SETTING_TIMESTAMP_OFFSET (DEST_LAST + 1)
// #define SETTING_HOST_BALANCE_OFFSET (DEST_LAST + 2)
enum {
	SETTING_NOTIFY_BATTLE  = 1 << 0,
	SETTING_NOTIFY_PRIVATE = 1 << 1,
	SETTING_NOTIFY_CHANNEL = 1 << 2,
	SETTING_NOTIFY_SERVER  = 1 << 3,
	SETTING_TIMESTAMP      = 1 << SETTING_TIMESTAMP_OFFSET,
	
	SETTING_HOST_BALANCE       = 1 << SETTING_HOST_BALANCE_OFFSET,
	SETTING_HOST_BALANCE_RANK  = 1 << (SETTING_HOST_BALANCE_OFFSET + 1),
	SETTING_HOST_BALANCE_CLAN = 1 << (SETTING_HOST_BALANCE_OFFSET + 2),
	SETTING_HOST_FIX_ID        = 1 << (SETTING_HOST_BALANCE_OFFSET + 3),
	SETTING_HOST_FIX_COLOR     = 1 << (SETTING_HOST_BALANCE_OFFSET + 4),
	
	SETTING_AUTOCONNECT = 1 << (SETTING_HOST_BALANCE_OFFSET + 5),
};

extern struct Settings {
	char *spring_path;
	int flags;
	char *autojoin;
	char *selected_packages;
} gSettings;

void OpenDefaultChannels(void);


wchar_t * GetDataDir(const wchar_t *file);
extern wchar_t gDataDir[MAX_PATH];

void SaveSetting(const char *key, const char *val);
char *LoadSetting(const char *key);
int LoadSettingInt(const char *key);
void SaveSettingInt(const char *key, int val);
void ResetSettings(void);

struct User;

void InitSettings(void);
void SaveAliases(void);

#endif /* end of include guard: SETTINGS_H */
