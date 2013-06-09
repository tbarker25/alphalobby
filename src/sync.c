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

#include <IL/il.h>
#include <assert.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <zlib.h>

#include <Shlobj.h>

#include "battle.h"
#include "battleroom.h"
#include "client_message.h"
#include "common.h"
#include "dialogboxes.h"
#include "downloader.h"
#include "iconlist.h"
#include "mainwindow.h"
#include "mybattle.h"
#include "settings.h"
#include "sync.h"
#include "user.h"

#define PLAIN_API_STRUCTURE
#define EXPORT2(type, name, args)\
	static type __stdcall (*name) args;
#include "unitsync_api.h"
#undef EXPORT2
#undef UNITSYNC_API_H

#define LENGTH(x) (sizeof(x) / sizeof(*x))

static const struct {
	void **proc;
	const char *name;
} x[] = {
#define EXPORT2(type, name, args)\
	{(void **)&name, #name},
#include "unitsync_api.h"
#undef EXPORT2
};

#define SYNCFILE_VERSION 0x03

typedef struct OptionList {
	Option *xs;
	size_t len;
}OptionList;

static void create_mod_file(const char *mod_name);
static void create_map_file(const char *map_name);

//Shared between threads:
static uint8_t taskReload, /* taskSetMinimap, */ /* taskSetInfo, */ taskSetBattleStatus;
static HANDLE event;

//malloc new value, swap atomically, and free old value:
static const char *mapToSave, *modToSave;
static char haveTriedToDownload;

//Sync thread only:
static char currentMod[MAX_TITLE];
static char currentMap[MAX_TITLE];

static void
print_last_error(wchar_t *title)
{
	wchar_t errorMessage[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	NULL, GetLastError(), 0, errorMessage, LENGTH(errorMessage), NULL);
	MessageBox(NULL, errorMessage, title, 0);
}


static DWORD WINAPI __attribute__((noreturn))
sync_proc (LPVOID lp_parameter)
{
	ilInit();

	HMODULE libUnitSync = LoadLibrary(L"unitsync.dll");
	if (!libUnitSync)
		print_last_error(L"Could not load unitsync.dll");

	for (int i=0; i<LENGTH(x); ++i) {
		*x[i].proc = GetProcAddress(libUnitSync, x[i].name);
		assert(*x[i].proc);
	}


	void *s;
	event = CreateEvent(NULL, FALSE, 0, NULL);
	taskReload=1;
	while (1) {
		if (taskReload) {
			taskReload = 0;
			Init(0, 0);

			taskSetBattleStatus = 1;

			for (int i=0; i<g_mod_count; ++i)
				free(g_mods[i]);
			free(g_mods);
			g_mod_count = GetPrimaryModCount();
			g_mods = malloc(g_mod_count * sizeof(g_mods[0]));
			for (int i=0; i<g_mod_count; ++i)
				g_mods[i] = _strdup(GetPrimaryModName(i));

			for (int i=0; i<g_map_count; ++i)
				free(g_maps[i]);
			free(g_maps);
			g_map_count = GetMapCount();
			g_maps = malloc(g_map_count * sizeof(g_maps[0]));
			for (int i=0; i<g_map_count; ++i)
				g_maps[i] = _strdup(GetMapName(i));

			void
reset_mapAndMod(void) {
				if (g_my_battle) {
					Sync_on_changed_mod(g_my_battle->mod_name);
					Sync_on_changed_map(g_my_battle->map_name);
				}
			}
			ExecuteInMainThread(reset_mapAndMod);
			haveTriedToDownload = 1;
		} else if ((s = (void *)__sync_fetch_and_and(&modToSave, NULL))) {
			create_mod_file(s);
			free(s);
		} else if ((s = (void *)__sync_fetch_and_and(&mapToSave, NULL))) {
			create_map_file(s);
			free(s);
		} else if (taskSetBattleStatus) {
			SetBattleStatus(&g_my_user, 0, 0);
			taskSetBattleStatus = 0;
		} else {
			WaitForSingleObject(event, INFINITE);
		}
	};
}

uint32_t
Sync_get_status(void)
{
	return (g_my_battle
			&& g_map_hash && (!g_my_battle->map_hash || g_map_hash == g_my_battle->map_hash)
			&& g_mod_hash && (!g_battle_options.modHash || g_mod_hash == g_battle_options.modHash))
		? SYNCED : UNSYNCED;
}

void
Sync_init(void)
{
	CreateThread(NULL, 0, sync_proc, NULL, 0, NULL);
}

static void
init_options(size_t nbOptions, gzFile fd)
{
	assert(nbOptions < 256);
	Option *options = calloc(10000, 1);
	char *s = (void *)&options[nbOptions];

	for (int i=0; i<nbOptions; ++i) {
		options[i].type = GetOptionType(i);
		assert(options[i].type != opt_error);

		options[i].key = s - (size_t)options;
		s += sprintf(s, "%s", GetOptionKey(i)) + 1;

		options[i].name = s - (size_t)options;
		s += sprintf(s, "%s", GetOptionName(i)) + 1;

		options[i].desc = s - (size_t)options;
		s += sprintf(s, "%s", GetOptionDesc(i)) + 1;

		options[i].def = s - (size_t)options;
		switch (GetOptionType(i)) {
		case opt_bool:
			*s++ = '0' + GetOptionBoolDef(i);
			*s++ = 0;
			break;
		case opt_number:
			s += sprintf(s, "%g", GetOptionNumberDef(i)) + 1;
			break;
		case opt_list:
			s += sprintf(s, "%s", GetOptionListDef(i)) + 1;
			options[i].nbListItems = GetOptionListCount(i);
			options[i].listItems = (void *)(s - (size_t)options);
			s += options[i].nbListItems * sizeof(*options[i].listItems);
			for (int j=0; j<options[i].nbListItems; ++j) {
				((OptionListItem *)((void *)&options[i].listItems[j] + (size_t)options))->key = s - (size_t)options;
				s += sprintf(s, "%s", GetOptionListItemKey(i, j)) + 1;
				((OptionListItem *)((void *)&options[i].listItems[j] + (size_t)options))->name = s - (size_t)options;
				s += sprintf(s, "%s", GetOptionListItemName(i, j)) + 1;
			}
			break;
		default:
			*s++ = 0;
			break;
		}
	}

	for (int i=0; i<nbOptions; ++i) {
		for (int j=0; j<nbOptions; ++j) {
			const char *section = GetOptionSection(i);
			if (options[j].type == opt_section
					&& !_stricmp(section, options[j].key + (size_t)options)) {
				options[i].section = (void *)j + 1;
			}
		}
	}
	size_t optionsSize = s - (char *)options;
	assert(optionsSize < 10000);

	gzwrite(fd, &optionsSize, 4);
	gzwrite(fd, &nbOptions, 4);

	gzwrite(fd, options, optionsSize);
	free(options);
}

static OptionList
load_options(gzFile fd)
{
	size_t optionsSize;
	size_t nbOptions;

	gzread(fd, &optionsSize, 4);
	gzread(fd, &nbOptions, 4);
	if (nbOptions == 0)
		return (OptionList){NULL, 0};

	assert(optionsSize < 10000);
	assert(nbOptions < 100);

	Option *options = calloc(optionsSize, 1);
	gzread(fd, options, optionsSize);

	for (int i=0; i<nbOptions; ++i) {
		options[i].key += (size_t)options;
		options[i].name += (size_t)options;
		options[i].desc += (size_t)options;
		options[i].def += (size_t)options;
		if (options[i].section)
			options[i].section = &options[(uintptr_t)options[i].section - 1];

		if (options[i].nbListItems) {
			options[i].listItems = ((void *)options[i].listItems + (size_t)options);
			for (int j=0; j<options[i].nbListItems; ++j) {
				options[i].listItems[j].key += (size_t)options;
				options[i].listItems[j].name += (size_t)options;
			}
		}
	}

	return (OptionList){options, nbOptions};
}

static void
create_map_file(const char *map_name)
{
	uint32_t map_hash = GetMapChecksumFromName(map_name);
	if (!map_hash) {
		if (!haveTriedToDownload)
			DownloadMap(map_name);
		return;
	}

	char tmp_file_path[MAX_PATH];
	GetTempPathA(MAX_PATH, tmp_file_path);
	GetTempFileNameA(tmp_file_path, NULL, 0, tmp_file_path);

	gzFile fd = gzopen(tmp_file_path, "wb");
	gzputc(fd, SYNCFILE_VERSION);
	gzwrite(fd, &map_hash, sizeof(map_hash));
	struct _LargeMapInfo mapInfo = {.mapInfo = {.description = mapInfo.description, .author = mapInfo.author}};

	GetMapInfoEx(map_name, &mapInfo.mapInfo, 1);
	gzwrite(fd, &mapInfo, sizeof(mapInfo));
	init_options(GetMapOptionCount(map_name), fd);

	uint16_t *minimapPixels = GetMinimap(map_name, MAP_DETAIL);
	assert(minimapPixels);
	gzwrite(fd, minimapPixels, MAP_SIZE*sizeof(*minimapPixels));

	for (int i=0; i<2; ++i) {
		const char *mapType = i ? "metal" : "height";
		int w=0, h=0;
		__attribute__((unused))
		int ok = GetInfoMapSize(map_name, mapType, &w, &h);
		assert(ok);
		void *mapData = calloc(1, w * h);
		gzwrite(fd, (uint16_t []){w, h}, 4);
		ok = GetInfoMap(map_name, mapType, mapData, bm_grayscale_8);
		assert(ok);
		gzwrite(fd, mapData, w * h);
		free(mapData);
	}

	gzclose(fd);

	char file_path[MAX_PATH];
	sprintf(file_path, "%lscache\\alphalobby\\%s.MapData", g_data_dir, map_name);
	SHCreateDirectoryExW(NULL, Settings_get_data_dir(L"cache\\alphalobby"), NULL);
	MoveFileExA(tmp_file_path, file_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);

	ExecuteInMainThread_param(Sync_on_changed_map, map_name);
}

static void
create_mod_file(const char *mod_name)
{
	RemoveAllArchives();
	GetPrimaryModCount(); //todo investigate if we only need it on reinit
	int modIndex = GetPrimaryModIndex(mod_name);
	if (modIndex < 0) {
		if (!haveTriedToDownload)
			DownloadMod(mod_name);
		return;
	}

	GetPrimaryModArchiveCount(modIndex);
	AddAllArchives(GetPrimaryModArchive(modIndex));

	char tmp_file_path[MAX_PATH];
	GetTempPathA(MAX_PATH, tmp_file_path);
	GetTempFileNameA(tmp_file_path, NULL, 0, tmp_file_path);

	gzFile fd = gzopen(tmp_file_path, "wb");
	gzputc(fd, SYNCFILE_VERSION);

	uint32_t modHash = GetPrimaryModChecksum(modIndex);
	gzwrite(fd, &modHash, sizeof(modHash));

	init_options(GetModOptionCount(), fd);

	uint8_t sideCount = GetSideCount();
	gzputc(fd, sideCount);
	char sideNames[sideCount][32];
	memset(sideNames, '\0', sideCount*32);
	for (uint8_t i=0; i<sideCount; ++i) {
		assert(strlen(GetSideName(i)) < 32);
		strcpy(sideNames[i], GetSideName(i));
	}
	gzwrite(fd, sideNames, sizeof(sideNames));

	uint32_t sidePics[sideCount][16*16];
	for (uint8_t i=0; i<sideCount; ++i) {
		char isBMP = 0;
		char vfs_path[128];
		int n = sprintf(vfs_path, "SidePics/%s.png", sideNames[i]);
		int fd = OpenFileVFS(vfs_path);
		if (!fd) {
			memcpy(&vfs_path[n - 3], (char[]){'b', 'm', 'p'}, 3);
			isBMP = 1;
			fd = OpenFileVFS(vfs_path);
		}
		if (!fd) {
			continue;
		}

		uint8_t buf[FileSizeVFS(fd)];
		ReadFileVFS(fd, buf, sizeof(buf));
		CloseFileVFS(fd);
		ilLoadL(IL_TYPE_UNKNOWN, buf, sizeof(buf));
		ilCopyPixels(0, 0, 0, 16, 16, 1, IL_BGRA, IL_UNSIGNED_BYTE, sidePics[i]);

		//Set white as transpareny for BMP:
		if (isBMP) {
			for (int j=0; j<16 * 16; ++j)
				if (sidePics[i][j] == 0xFFFFFFFF)
					sidePics[i][j] &= 0x00FFFFFF;
		}
	}
	gzwrite(fd, sidePics, sizeof(sidePics));

	gzclose(fd);

	char file_path[MAX_PATH];
	sprintf(file_path, "%lscache\\alphalobby\\%s.ModData", g_data_dir, mod_name);
	SHCreateDirectoryExW(NULL, Settings_get_data_dir(L"cache\\alphalobby"), NULL);
	MoveFileExA(tmp_file_path, file_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);

	ExecuteInMainThread_param(Sync_on_changed_mod, mod_name);
}

void
Sync_on_changed_mod(const char *mod_name)
{
	char file_path[MAX_PATH];
	gzFile fd;
	OptionList modOptionList;

	assert(GetCurrentThreadId() == GetWindowThreadProcessId(g_main_window, NULL));
	if (!_stricmp(currentMod, mod_name))
		return;

	for (int i=0; i<g_mod_option_count; ++i)
		free(g_mod_options[i].val);
	free(g_mod_options);
	g_mod_options = NULL;
	g_mod_option_count = 0;

	sprintf(file_path, "%lscache\\alphalobby\\%s.ModData", g_data_dir, mod_name);
	fd = gzopen(file_path, "rb");

	if (fd && gzgetc(fd) != SYNCFILE_VERSION) {
		gzclose(fd);
		remove(file_path);
		fd = 0;
	}
	if (!fd) {
		haveTriedToDownload = 0;
		g_side_count = 0;
		g_mod_hash = 0;
		currentMod[0] = 0;
		free(InterlockedExchangePointer(&modToSave, _strdup(mod_name)));
		SetEvent(event);
		MyBattle_update_mod_options();
		return;
	}
	strcpy(currentMod, mod_name);
	gzread(fd, &g_mod_hash, sizeof(g_mod_hash));
	modOptionList = load_options(fd);
	g_mod_options = modOptionList.xs;
	g_mod_option_count = modOptionList.len;
	g_side_count = gzgetc(fd);
	if (g_side_count > 0) {
		uint32_t sidePics[g_side_count][16*16];

		gzread(fd, g_side_names, g_side_count * 32);
		gzread(fd, sidePics, sizeof(sidePics));

		for (int i=0; i<g_side_count; ++i) {
			HBITMAP bitmap = CreateBitmap(16, 16, 1, 32, sidePics[i]);
			ImageList_Replace(g_icon_list, ICONS_FIRST_SIDE + i, bitmap, NULL);
			DeleteObject(bitmap);
		}
	}
	gzclose(fd);
	BattleRoom_on_change_mod();
	MyBattle_update_mod_options();
	taskSetBattleStatus = 1;
	SetEvent(event);
}

void
Sync_on_changed_map(const char *map_name)
{
	assert(GetCurrentThreadId() == GetWindowThreadProcessId(g_main_window, NULL));

	if (!_stricmp(currentMap, map_name))
		return;

	for (int i=0; i<g_map_option_count; ++i)
		free(g_map_options[i].val);
	free(g_map_options);
	g_map_options = NULL;
	g_map_option_count = 0;

	char file_path[MAX_PATH];
	sprintf(file_path, "%lscache\\alphalobby\\%s.MapData", g_data_dir, map_name);

	gzFile fd = NULL;

	for (int i=0; i<g_map_count; ++i) {
		if (!strcmp(map_name, g_maps[i])) {
			fd = gzopen(file_path, "rb");
			break;
		}
	}

	if (fd && gzgetc(fd) != SYNCFILE_VERSION) {
		gzclose(fd);
		remove(file_path);
		fd = 0;
	}

	if (!fd) {
		haveTriedToDownload = 0;
		g_map_hash = 0;
		currentMap[0] = 0;
		BattleRoom_change_minimap_bitmap(NULL, 0, 0, NULL, 0, 0, NULL);
		free(InterlockedExchangePointer(&mapToSave, _strdup(map_name)));
		SetEvent(event);
		MyBattle_update_mod_options();
		return;
	}

	strcpy(currentMap, map_name);

	gzread(fd, &g_map_hash, sizeof(g_map_hash));
	gzread(fd, &_gLargeMapInfo, sizeof(_gLargeMapInfo));
	g_map_info.description = _gLargeMapInfo.description;
	g_map_info.author = _gLargeMapInfo.author;

	OptionList optionList = load_options(fd);
	g_map_options = optionList.xs;
	g_map_option_count = optionList.len;

	//Texture pixels:
	static uint16_t *pixels;
	if (!pixels)
		pixels = malloc(MAP_SIZE * sizeof(*pixels));

	gzread(fd, pixels,MAP_SIZE * sizeof(pixels[0]));

	//Heightmap pixels:
	static uint8_t *heightMapPixels;
	free(heightMapPixels);
	uint16_t h[2];
	gzread(fd, h, 4);
	heightMapPixels = malloc(h[0] * h[1]);
	gzread(fd, heightMapPixels, h[0] *  h[1]);

	//Metalmap pixels:
	static uint8_t *metalMapPixels;
	free(metalMapPixels);
	uint16_t d[2];
	gzread(fd, d, 4);
	metalMapPixels = malloc(d[0] * d[1]);
	gzread(fd, metalMapPixels, d[0] *  d[1]);

	BattleRoom_change_minimap_bitmap(pixels,
			d[0], d[1], metalMapPixels,
			h[0], h[1], heightMapPixels);

	gzclose(fd);

	// taskSetMinimap = 1;
	MyBattle_update_mod_options();
	taskSetBattleStatus = 1;
	SetEvent(event);
}

void
Sync_reload(void)
{
	g_mod_hash = 0;
	currentMod[0] = 0;
	g_map_hash = 0;
	currentMap[0] = 0;
	taskReload = 1;
	SetEvent(event);
}

uint32_t
Sync_map_hash(const char *map_name)
{
	return GetMapChecksumFromName(map_name);
}

uint32_t
Sync_mod_hash(const char *mod_name)
{
	return GetPrimaryModChecksumFromName(mod_name);
}

const char *
Sync_spring_version(void)
{
	return GetSpringVersion();
}

void
Sync_for_each_ai(void (*func)(const char *, void *), void *arg)
{
	for (int imax=GetSkirmishAICount(), i=0; i<imax; ++i) {
		char aiName[128]; size_t len=0;
		for (int jmax = GetSkirmishAIInfoCount(i), j=0; j<jmax; ++j)
			if (!strcmp(GetInfoKey(j), "shortName"))
				len = sprintf(aiName, GetInfoValue(j));
			else if (!strcmp(GetInfoKey(j), "version"))
				sprintf(aiName + len, "|%s", GetInfoValue(j));
		func(aiName, arg);
	}
}

int
Sync_ai_option_count(const char *name)
{
	const char *name_len = strchr(name, '|');
	for (int imax=GetSkirmishAICount(), i=0; i<imax; ++i) {
		for (int jmax = GetSkirmishAIInfoCount(i), j=0; j<jmax; ++j) {
			if ((!strcmp("shortName", GetInfoKey(j))
						&& strncmp(name, GetInfoValue(j), name_len - name))
					|| (!strcmp(GetInfoKey(j), "version")
						&& strcmp(name_len+1, GetInfoValue(j))))
				goto nextAi;
		}
		return GetSkirmishAIOptionCount(i);
nextAi:;
	}
	return -1;
}


void CALLBACK
Sync_add_replays_to_listview(HWND listViewWindow)
{
	int handle = InitFindVFS("demos/*.sdf");
	if (handle < 0) //Init_fileVFS has a different error return than FindFilesVFS
		return;
	do {
		char buf[1024];
		handle = FindFilesVFS(handle, buf, sizeof(buf));
		SendMessageA(listViewWindow, LVM_INSERTITEMA, 0, (LPARAM)&(LVITEMA){LVIF_TEXT, 0, .pszText = buf + sizeof("demos/") - 1});
	} while (handle);
}

void
Sync_cleanup(void)
{
	if (currentMap[0])
		Settings_save_str("last_map", currentMap);
	if (currentMod[0])
		Settings_save_str("last_mod", currentMod);
}
