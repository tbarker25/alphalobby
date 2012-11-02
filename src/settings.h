#pragma once

#define SETTING_TIMESTAMP_OFFSET (DEST_LAST + 1)
#define SETTING_HOST_BALANCE_OFFSET (DEST_LAST + 2)
enum {
	SETTING_NOTIFY_BATTLE  = 1 << DEST_BATTLE,
	SETTING_NOTIFY_PRIVATE = 1 << DEST_PRIVATE,
	SETTING_NOTIFY_CHANNEL = 1 << DEST_CHANNEL,
	SETTING_NOTIFY_SERVER  = 1 << DEST_SERVER,
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
