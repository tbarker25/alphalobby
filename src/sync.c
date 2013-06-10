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
static uint8_t task_reload, /* task_set_minimap, */ /* task_set_info, */ task_set_battle_status;
static HANDLE event;

//malloc new value, swap atomically, and free old value:
static const char *map_to_save, *mod_to_save;
static char have_tried_to_download;

//Sync thread only:
static char current_mod[MAX_TITLE];
static char current_map[MAX_TITLE];

static void
print_last_error(wchar_t *title)
{
	wchar_t error_message[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	NULL, GetLastError(), 0, error_message, LENGTH(error_message), NULL);
	MessageBox(NULL, error_message, title, 0);
}


static DWORD WINAPI __attribute__((noreturn))
syncProc (LPVOID lp_parameter)
{
	ilInit();

	HMODULE lib_unit_sync = LoadLibrary(L"unitsync.dll");
	if (!lib_unit_sync)
		print_last_error(L"Could not load unitsync.dll");

	for (int i=0; i<LENGTH(x); ++i) {
		*x[i].proc = GetProcAddress(lib_unit_sync, x[i].name);
		assert(*x[i].proc);
	}


	void *s;
	event = CreateEvent(NULL, FALSE, 0, NULL);
	task_reload=1;
	while (1) {
		if (task_reload) {
			task_reload = 0;
			Init(0, 0);

			task_set_battle_status = 1;

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
reset_map_and_mod(void) {
				if (g_my_battle) {
					Sync_on_changed_mod(g_my_battle->mod_name);
					Sync_on_changed_map(g_my_battle->map_name);
				}
			}
			ExecuteInMainThread(reset_map_and_mod);
			have_tried_to_download = 1;
		} else if ((s = (void *)__sync_fetch_and_and(&mod_to_save, NULL))) {
			create_mod_file(s);
			free(s);
		} else if ((s = (void *)__sync_fetch_and_and(&map_to_save, NULL))) {
			create_map_file(s);
			free(s);
		} else if (task_set_battle_status) {
			SetBattleStatus(&g_my_user, 0, 0);
			task_set_battle_status = 0;
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
			&& g_mod_hash && (!g_battle_options.mod_hash || g_mod_hash == g_battle_options.mod_hash))
		? SYNCED : UNSYNCED;
}

void
Sync_init(void)
{
	CreateThread(NULL, 0, syncProc, NULL, 0, NULL);
}

static void
init_options(size_t option_count, gzFile fd)
{
	assert(option_count < 256);
	Option *options = calloc(10000, 1);
	char *s = (void *)&options[option_count];

	for (int i=0; i<option_count; ++i) {
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
			options[i].list_item_count = GetOptionListCount(i);
			options[i].list_items = (void *)(s - (size_t)options);
			s += options[i].list_item_count * sizeof(*options[i].list_items);
			for (int j=0; j<options[i].list_item_count; ++j) {
				((OptionListItem *)((void *)&options[i].list_items[j] + (size_t)options))->key = s - (size_t)options;
				s += sprintf(s, "%s", GetOptionListItemKey(i, j)) + 1;
				((OptionListItem *)((void *)&options[i].list_items[j] + (size_t)options))->name = s - (size_t)options;
				s += sprintf(s, "%s", GetOptionListItemName(i, j)) + 1;
			}
			break;
		default:
			*s++ = 0;
			break;
		}
	}

	for (int i=0; i<option_count; ++i) {
		for (int j=0; j<option_count; ++j) {
			const char *section = GetOptionSection(i);
			if (options[j].type == opt_section
					&& !_stricmp(section, options[j].key + (size_t)options)) {
				options[i].section = (void *)j + 1;
			}
		}
	}
	size_t options_size = s - (char *)options;
	assert(options_size < 10000);

	gzwrite(fd, &options_size, 4);
	gzwrite(fd, &option_count, 4);

	gzwrite(fd, options, options_size);
	free(options);
}

static OptionList
load_options(gzFile fd)
{
	size_t options_size;
	size_t option_count;

	gzread(fd, &options_size, 4);
	gzread(fd, &option_count, 4);
	if (option_count == 0)
		return (OptionList){NULL, 0};

	assert(options_size < 10000);
	assert(option_count < 100);

	Option *options = calloc(options_size, 1);
	gzread(fd, options, options_size);

	for (int i=0; i<option_count; ++i) {
		options[i].key += (size_t)options;
		options[i].name += (size_t)options;
		options[i].desc += (size_t)options;
		options[i].def += (size_t)options;
		if (options[i].section)
			options[i].section = &options[(uintptr_t)options[i].section - 1];

		if (options[i].list_item_count) {
			options[i].list_items = ((void *)options[i].list_items + (size_t)options);
			for (int j=0; j<options[i].list_item_count; ++j) {
				options[i].list_items[j].key += (size_t)options;
				options[i].list_items[j].name += (size_t)options;
			}
		}
	}

	return (OptionList){options, option_count};
}

static void
create_map_file(const char *map_name)
{
	uint32_t map_hash = GetMapChecksumFromName(map_name);
	if (!map_hash) {
		if (!have_tried_to_download)
			DownloadMap(map_name);
		return;
	}

	char tmp_file_path[MAX_PATH];
	GetTempPathA(MAX_PATH, tmp_file_path);
	GetTempFileNameA(tmp_file_path, NULL, 0, tmp_file_path);

	gzFile fd = gzopen(tmp_file_path, "wb");
	gzputc(fd, SYNCFILE_VERSION);
	gzwrite(fd, &map_hash, sizeof(map_hash));
	struct _LargeMapInfo map_info = {.map_info = {.description = map_info.description, .author = map_info.author}};

	GetMapInfoEx(map_name, &map_info.map_info, 1);
	gzwrite(fd, &map_info, sizeof(map_info));
	init_options(GetMapOptionCount(map_name), fd);

	uint16_t *minimap_pixels = GetMinimap(map_name, MAP_DETAIL);
	assert(minimap_pixels);
	gzwrite(fd, minimap_pixels, MAP_SIZE*sizeof(*minimap_pixels));

	for (int i=0; i<2; ++i) {
		const char *map_type = i ? "metal" : "height";
		int w=0, h=0;
		__attribute__((unused))
		int ok = GetInfoMapSize(map_name, map_type, &w, &h);
		assert(ok);
		void *map_data = calloc(1, w * h);
		gzwrite(fd, (uint16_t []){w, h}, 4);
		ok = GetInfoMap(map_name, map_type, map_data, bm_grayscale_8);
		assert(ok);
		gzwrite(fd, map_data, w * h);
		free(map_data);
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
	int mod_index = GetPrimaryModIndex(mod_name);
	if (mod_index < 0) {
		if (!have_tried_to_download)
			DownloadMod(mod_name);
		return;
	}

	GetPrimaryModArchiveCount(mod_index);
	AddAllArchives(GetPrimaryModArchive(mod_index));

	char tmp_file_path[MAX_PATH];
	GetTempPathA(MAX_PATH, tmp_file_path);
	GetTempFileNameA(tmp_file_path, NULL, 0, tmp_file_path);

	gzFile fd = gzopen(tmp_file_path, "wb");
	gzputc(fd, SYNCFILE_VERSION);

	uint32_t mod_hash = GetPrimaryModChecksum(mod_index);
	gzwrite(fd, &mod_hash, sizeof(mod_hash));

	init_options(GetModOptionCount(), fd);

	uint8_t side_count = GetSideCount();
	gzputc(fd, side_count);
	char side_names[side_count][32];
	memset(side_names, '\0', side_count*32);
	for (uint8_t i=0; i<side_count; ++i) {
		assert(strlen(GetSideName(i)) < 32);
		strcpy(side_names[i], GetSideName(i));
	}
	gzwrite(fd, side_names, sizeof(side_names));

	uint32_t side_pics[side_count][16*16];
	for (uint8_t i=0; i<side_count; ++i) {
		char is_b_m_p = 0;
		char vfs_path[128];
		int n = sprintf(vfs_path, "SidePics/%s.png", side_names[i]);
		int fd = OpenFileVFS(vfs_path);
		if (!fd) {
			memcpy(&vfs_path[n - 3], (char[]){'b', 'm', 'p'}, 3);
			is_b_m_p = 1;
			fd = OpenFileVFS(vfs_path);
		}
		if (!fd) {
			continue;
		}

		uint8_t buf[FileSizeVFS(fd)];
		ReadFileVFS(fd, buf, sizeof(buf));
		CloseFileVFS(fd);
		ilLoadL(IL_TYPE_UNKNOWN, buf, sizeof(buf));
		ilCopyPixels(0, 0, 0, 16, 16, 1, IL_BGRA, IL_UNSIGNED_BYTE, side_pics[i]);

		//Set white as transpareny for BMP:
		if (is_b_m_p) {
			for (int j=0; j<16 * 16; ++j)
				if (side_pics[i][j] == 0xFFFFFFFF)
					side_pics[i][j] &= 0x00FFFFFF;
		}
	}
	gzwrite(fd, side_pics, sizeof(side_pics));

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
	OptionList mod_option_list;

	assert(GetCurrentThreadId() == GetWindowThreadProcessId(g_main_window, NULL));
	if (!_stricmp(current_mod, mod_name))
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
		have_tried_to_download = 0;
		g_side_count = 0;
		g_mod_hash = 0;
		current_mod[0] = 0;
		free(InterlockedExchangePointer(&mod_to_save, _strdup(mod_name)));
		SetEvent(event);
		MyBattle_update_mod_options();
		return;
	}
	strcpy(current_mod, mod_name);
	gzread(fd, &g_mod_hash, sizeof(g_mod_hash));
	mod_option_list = load_options(fd);
	g_mod_options = mod_option_list.xs;
	g_mod_option_count = mod_option_list.len;
	g_side_count = gzgetc(fd);
	if (g_side_count > 0) {
		uint32_t side_pics[g_side_count][16*16];

		gzread(fd, g_side_names, g_side_count * 32);
		gzread(fd, side_pics, sizeof(side_pics));

		for (int i=0; i<g_side_count; ++i) {
			HBITMAP bitmap = CreateBitmap(16, 16, 1, 32, side_pics[i]);
			ImageList_Replace(g_icon_list, ICONS_FIRST_SIDE + i, bitmap, NULL);
			DeleteObject(bitmap);
		}
	}
	gzclose(fd);
	BattleRoom_on_change_mod();
	MyBattle_update_mod_options();
	task_set_battle_status = 1;
	SetEvent(event);
}

void
Sync_on_changed_map(const char *map_name)
{
	assert(GetCurrentThreadId() == GetWindowThreadProcessId(g_main_window, NULL));

	if (!_stricmp(current_map, map_name))
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
		have_tried_to_download = 0;
		g_map_hash = 0;
		current_map[0] = 0;
		BattleRoom_change_minimap_bitmap(NULL, 0, 0, NULL, 0, 0, NULL);
		free(InterlockedExchangePointer(&map_to_save, _strdup(map_name)));
		SetEvent(event);
		MyBattle_update_mod_options();
		return;
	}

	strcpy(current_map, map_name);

	gzread(fd, &g_map_hash, sizeof(g_map_hash));
	gzread(fd, &_g_largeMapInfo, sizeof(_g_largeMapInfo));
	g_map_info.description = _g_largeMapInfo.description;
	g_map_info.author = _g_largeMapInfo.author;

	OptionList option_list = load_options(fd);
	g_map_options = option_list.xs;
	g_map_option_count = option_list.len;

	//Texture pixels:
	static uint16_t *pixels;
	if (!pixels)
		pixels = malloc(MAP_SIZE * sizeof(*pixels));

	gzread(fd, pixels,MAP_SIZE * sizeof(pixels[0]));

	//Heightmap pixels:
	static uint8_t *height_map_pixels;
	free(height_map_pixels);
	uint16_t h[2];
	gzread(fd, h, 4);
	height_map_pixels = malloc(h[0] * h[1]);
	gzread(fd, height_map_pixels, h[0] *  h[1]);

	//Metalmap pixels:
	static uint8_t *metal_map_pixels;
	free(metal_map_pixels);
	uint16_t d[2];
	gzread(fd, d, 4);
	metal_map_pixels = malloc(d[0] * d[1]);
	gzread(fd, metal_map_pixels, d[0] *  d[1]);

	BattleRoom_change_minimap_bitmap(pixels,
			d[0], d[1], metal_map_pixels,
			h[0], h[1], height_map_pixels);

	gzclose(fd);

	// task_set_minimap = 1;
	MyBattle_update_mod_options();
	task_set_battle_status = 1;
	SetEvent(event);
}

void
Sync_reload(void)
{
	g_mod_hash = 0;
	current_mod[0] = 0;
	g_map_hash = 0;
	current_map[0] = 0;
	task_reload = 1;
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
			if (!strcmp(GetInfoKey(j), "short_name"))
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
			if ((!strcmp("short_name", GetInfoKey(j))
						&& strncmp(name, GetInfoValue(j), name_len - name))
					|| (!strcmp(GetInfoKey(j), "version")
						&& strcmp(name_len+1, GetInfoValue(j))))
				goto next_ai;
		}
		return GetSkirmishAIOptionCount(i);
next_ai:;
	}
	return -1;
}


void CALLBACK
Sync_add_replays_to_listview(HWND list_view_window)
{
	int handle = InitFindVFS("demos/*.sdf");
	if (handle < 0) //Init_file_vFS has a different error return than FindFilesVFS
		return;
	do {
		char buf[1024];
		handle = FindFilesVFS(handle, buf, sizeof(buf));
		SendMessageA(list_view_window, LVM_INSERTITEMA, 0, (LPARAM)&(LVITEMA){LVIF_TEXT, 0, .pszText = buf + sizeof("demos/") - 1});
	} while (handle);
}

void
Sync_cleanup(void)
{
	if (current_map[0])
		Settings_save_str("last_map", current_map);
	if (current_mod[0])
		Settings_save_str("last_mod", current_mod);
}
