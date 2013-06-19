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

wchar_t * Settings_get_data_dir(const wchar_t *file);
void      Settings_init(void);
int       Settings_load_int(const char *key);
char *    Settings_load_str(const char *key);
bool      Settings_load_str2(char *buf, const char *key);
void      Settings_open_default_channels(void);
void      Settings_reset(void);
void      Settings_save_aliases(void);
void      Settings_save_int(const char *key, int val);
void      Settings_save_str(const char *key, const char *val);

extern struct Settings {
	char *spring_path;
	int flags;
	char *autojoin;
	char *selected_packages;
} g_settings;

extern wchar_t g_data_dir[MAX_PATH];

#endif /* end of include guard: SETTINGS_H */
