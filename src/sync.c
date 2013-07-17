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
#include "tasserver.h"
#include "common.h"
#include "downloader.h"
#include "iconlist.h"
#include "mainwindow.h"
#include "minimap.h"
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

#define LENGTH(x) (sizeof x / sizeof *x)
#define swap(_a, _b) {typeof(_a) tmp = _a; _a = _b; _b = tmp;}

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
	int len;
}OptionList;

static void            create_mod_file  (const char *mod_name);
static void            create_map_file  (const char *map_name);
static void            init_options     (int option_len, gzFile);
static OptionList      load_options     (gzFile);
static void            print_last_error (const wchar_t *title);
static void            reload           (void);
static uint32_t WINAPI syncProc         (void *unused);
static void            write_info_map   (const char *map_name, const char *map_type, gzFile);

//Shared between threads:
static uint8_t s_task_reload, /* task_set_minimap, */ /* task_set_info, */ s_task_set_status;
static HANDLE s_event;

//malloc new value, swap atomically, and free old value:
static const char *s_map_to_save, *s_mod_to_save;
static char s_have_tried_download;

//Sync thread only:
static char s_current_mod[MAX_TITLE];
static char s_current_map[MAX_TITLE];

static void
print_last_error(const wchar_t *title)
{
	wchar_t error_message[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	    NULL, GetLastError(), 0, error_message, LENGTH(error_message), NULL);
	MessageBox(NULL, error_message, (wchar_t *)title, 0);
}

static uint32_t WINAPI __attribute__((noreturn))
syncProc (__attribute__((unused)) void *unused)
{
	ilInit();

	HMODULE lib_unit_sync = LoadLibrary(L"unitsync.dll");
	if (!lib_unit_sync)
		print_last_error(L"Could not load unitsync.dll");

	for (size_t i=0; i<LENGTH(x); ++i) {
		*x[i].proc = GetProcAddress(lib_unit_sync, x[i].name);
		assert(*x[i].proc);
	}

	void *s;
	s_event = CreateEvent(NULL, FALSE, 0, NULL);
	s_task_reload=1;
	while (1) {
		if (s_task_reload) {
			reload();
			s_task_reload = 0;
			s_task_set_status = 1;
			s_have_tried_download = 1;
		} else if ((s = (void *)__sync_fetch_and_and(&s_mod_to_save, NULL))) {
			create_mod_file(s);
			free(s);
		} else if ((s = (void *)__sync_fetch_and_and(&s_map_to_save, NULL))) {
			create_map_file(s);
			free(s);
		} else if (s_task_set_status) {
			TasServer_send_my_battle_status(g_last_battle_status);
			s_task_set_status = 0;
		} else {
			WaitForSingleObject(s_event, INFINITE);
		}
	};
}

uint32_t
Sync_is_synced(void)
{
	return g_my_battle && g_map_hash && (!g_my_battle->map_hash || g_map_hash == g_my_battle->map_hash)
		&& g_mod_hash && (!g_battle_options.mod_hash || g_mod_hash == g_battle_options.mod_hash);
}

void
Sync_init(void)
{
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)syncProc, NULL, 0, NULL);
}

static void
reload(void)
{
	char **mods;
	char **maps;
	int mods_len;
	int maps_len;

	Init(0, 0);

	mods_len = GetPrimaryModCount();
	mods = malloc((size_t)mods_len * sizeof mods[0]);
	for (int i=0; i<mods_len; ++i)
		mods[i] = _strdup(GetPrimaryModName(i));

	maps_len = GetMapCount();
	maps = malloc((size_t)maps_len * sizeof maps[0]);
	for (int i=0; i<maps_len; ++i)
		maps[i] = _strdup(GetMapName(i));

	void reset_map_and_mod(void) {
		swap(mods, g_mods);
		swap(maps, g_maps);
		swap(mods_len, g_mod_len);
		swap(maps_len, g_map_len);
		if (g_my_battle) {
			Sync_on_changed_mod(g_my_battle->mod_name);
			Sync_on_changed_map(g_my_battle->map_name);
		}
	}
	ExecuteInMainThread(reset_map_and_mod);
	for (int i=0; i<mods_len; ++i)
		free(mods[i]);
	free(mods);
	for (int i=0; i<maps_len; ++i)
		free(maps[i]);
	free(maps);
}

static void
init_options(int option_len, gzFile fd)
{
	assert(option_len < 256);
	Option *options = calloc(10000, 1);
	char *s = (void *)&options[option_len];

	for (int i=0; i<option_len; ++i) {
		options[i].type = GetOptionType(i);
		assert(options[i].type != opt_error);

		options[i].key = s - (uintptr_t)options;
		s += sprintf(s, "%s", GetOptionKey(i)) + 1;

		options[i].name = s - (uintptr_t)options;
		s += sprintf(s, "%s", GetOptionName(i)) + 1;

		options[i].desc = s - (uintptr_t)options;
		s += sprintf(s, "%s", GetOptionDesc(i)) + 1;

		options[i].def = s - (uintptr_t)options;
		switch (GetOptionType(i)) {
		case opt_bool:
			*s++ = (char)('0' + GetOptionBoolDef(i));
			*s++ = 0;
			break;
		case opt_number:
			s += sprintf(s, "%g", GetOptionNumberDef(i)) + 1;
			break;
		case opt_list:
			s += sprintf(s, "%s", GetOptionListDef(i)) + 1;
			options[i].list_item_len = GetOptionListCount(i);
			options[i].list_items = (OptionListItem *)(s - (uintptr_t)options);
			s += (size_t)options[i].list_item_len * sizeof *options[i].list_items;
			for (int j=0; j<options[i].list_item_len; ++j) {
				OptionListItem *item = (OptionListItem *)((char *)&options[i].list_items[j] + (uintptr_t)options);
				item->key = s - (uintptr_t)options;
				s += sprintf(s, "%s", GetOptionListItemKey(i, j)) + 1;
				item->name = s - (uintptr_t)options;
				s += sprintf(s, "%s", GetOptionListItemName(i, j)) + 1;
			}
			break;
		default:
			*s++ = 0;
			break;
		}
	}

	for (int i=0; i<option_len; ++i) {
		for (int j=0; j<option_len; ++j) {
			const char *section = GetOptionSection(i);
			if (options[j].type == opt_section
			    && !_stricmp(section, options[j].key + (size_t)options)) {
				options[i].section = (Option *)((uintptr_t)j + 1);
			}
		}
	}
	size_t options_size = (size_t)(s - (uintptr_t)options);
	assert(options_size < 10000);

	gzwrite(fd, &options_size, 4);
	gzwrite(fd, &option_len, 4);

	gzwrite(fd, options, options_size);
	free(options);
}

static OptionList
load_options(gzFile fd)
{
	int32_t options_size;
	int32_t option_len;

	gzread(fd, &options_size, sizeof options_size);
	gzread(fd, &option_len, sizeof option_len);
	if (option_len == 0)
		return (OptionList){NULL, 0};

	assert(options_size < 10000 && options_size > 0);
	assert(option_len < 100 && option_len > 0);

	Option *options = calloc(1, (size_t)options_size);
	gzread(fd, options, (size_t)options_size);

	for (int i=0; i<option_len; ++i) {
		options[i].key += (size_t)options;
		options[i].name += (size_t)options;
		options[i].desc += (size_t)options;
		options[i].def += (size_t)options;
		if (options[i].section)
			options[i].section = &options[(uintptr_t)options[i].section - 1];

		if (options[i].list_item_len) {
			options[i].list_items = (OptionListItem *)((uintptr_t)options[i].list_items + (uintptr_t)options);
			for (int j=0; j<options[i].list_item_len; ++j) {
				options[i].list_items[j].key += (size_t)options;
				options[i].list_items[j].name += (size_t)options;
			}
		}
	}

	return (OptionList){options, option_len};
}

static void
write_info_map(const char *map_name, const char *map_type, gzFile gz_file)
{
	int w=0, h=0;
	uint32_t tmp;
	void *map_data;
	int ok __attribute__((unused));

	ok = GetInfoMapSize(map_name, map_type, &w, &h);
	assert(ok);

	tmp = (uint32_t)(h << 16 | w);
	gzwrite(gz_file, &tmp, sizeof tmp);

	map_data = calloc(1, (size_t)(w * h));
	ok = GetInfoMap(map_name, map_type, map_data, bm_grayscale_8);
	assert(ok);

	gzwrite(gz_file, map_data, (size_t)(w * h));
	free(map_data);
}

static void
create_map_file(const char *map_name)
{
	uint32_t map_hash;
	uint16_t *minimap_pixels;
	char file_path[MAX_PATH];
	char tmp_file_path[MAX_PATH];
	struct MapInfo_ map_info;

	map_hash = GetMapChecksumFromName(map_name);

	if (!map_hash) {
		if (!s_have_tried_download)
			DownloadMap(map_name);
		return;
	}

	GetTempPathA(MAX_PATH, tmp_file_path);
	GetTempFileNameA(tmp_file_path, NULL, 0, tmp_file_path);

	gzFile gz_file = gzopen(tmp_file_path, "wb");
	gzputc(gz_file, SYNCFILE_VERSION);
	gzwrite(gz_file, &map_hash, sizeof map_hash);
	map_info.description = map_info._description;
	map_info.author = map_info._author;

	GetMapInfoEx(map_name, &map_info, 1);
	gzwrite(gz_file, &map_info, sizeof map_info);
	init_options(GetMapOptionCount(map_name), gz_file);

	minimap_pixels = GetMinimap(map_name, MAP_DETAIL);
	assert(minimap_pixels);
	gzwrite(gz_file, minimap_pixels, MAP_SIZE*sizeof *minimap_pixels);

	write_info_map(map_name, "height", gz_file);
	write_info_map(map_name, "metal", gz_file);

	gzclose(gz_file);

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
		if (!s_have_tried_download)
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
	gzwrite(fd, &mod_hash, sizeof mod_hash);

	init_options(GetModOptionCount(), fd);

	uint8_t side_len = (uint8_t)GetSideCount();
	gzputc(fd, side_len);
	char side_names[side_len][32];
	memset(side_names, '\0', (size_t)(side_len*32));
	for (uint8_t i=0; i<side_len; ++i) {
		assert(strlen(GetSideName(i)) < 32);
		strcpy(side_names[i], GetSideName(i));
	}
	gzwrite(fd, side_names, sizeof side_names);

	uint32_t side_pics[side_len][16*16];
	for (uint8_t i=0; i<side_len; ++i) {
		bool is_bmp = false;
		char vfs_path[128];
		int n = sprintf(vfs_path, "SidePics/%s.png", side_names[i]);
		int sidepic_fd = OpenFileVFS(vfs_path);
		if (!sidepic_fd) {
			vfs_path[n - 3] = 'b';
			vfs_path[n - 2] = 'm';
			vfs_path[n - 1] = 'p';
			is_bmp = true;
			sidepic_fd = OpenFileVFS(vfs_path);
		}

		if (!sidepic_fd)
			continue;

		uint8_t buf[FileSizeVFS(sidepic_fd)];
		ReadFileVFS(sidepic_fd, buf, (int)sizeof buf);
		CloseFileVFS(sidepic_fd);
		ilLoadL(IL_TYPE_UNKNOWN, buf, sizeof buf);
		ilCopyPixels(0, 0, 0, 16, 16, 1, IL_BGRA, IL_UNSIGNED_BYTE, side_pics[i]);

		//Set white as transpareny for BMP:
		if (is_bmp) {
			for (int j=0; j<16 * 16; ++j)
				if (side_pics[i][j] == 0xFFFFFFFF)
					side_pics[i][j] &= 0x00FFFFFF;
		}
	}
	gzwrite(fd, side_pics, sizeof side_pics);

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
	if (!_stricmp(s_current_mod, mod_name))
		return;

	for (int i=0; i<g_mod_option_len; ++i)
		free(g_mod_options[i].val);
	free(g_mod_options);
	g_mod_options = NULL;
	g_mod_option_len = 0;

	sprintf(file_path, "%lscache\\alphalobby\\%s.ModData", g_data_dir, mod_name);
	fd = gzopen(file_path, "rb");

	if (fd && gzgetc(fd) != SYNCFILE_VERSION) {
		gzclose(fd);
		remove(file_path);
		fd = 0;
	}
	if (!fd) {
		s_have_tried_download = 0;
		g_side_len = 0;
		g_mod_hash = 0;
		s_current_mod[0] = 0;
		free(InterlockedExchangePointer(&s_mod_to_save, _strdup(mod_name)));
		SetEvent(s_event);
		MyBattle_update_mod_options();
		return;
	}
	strcpy(s_current_mod, mod_name);
	gzread(fd, &g_mod_hash, sizeof g_mod_hash);
	mod_option_list = load_options(fd);
	g_mod_options = mod_option_list.xs;
	g_mod_option_len = mod_option_list.len;
	g_side_len = (uint8_t)gzgetc(fd);
	if (g_side_len > 0) {
		uint32_t side_pics[g_side_len][16*16];

		gzread(fd, g_side_names, g_side_len * 32u);
		gzread(fd, side_pics, sizeof side_pics);

		for (uint8_t i=0; i<g_side_len; ++i) {
			HBITMAP bitmap = CreateBitmap(16, 16, 1, 32, side_pics[i]);
			IconList_replace_icon(ICON_FIRST_SIDE + i, bitmap);
			DeleteObject(bitmap);
		}
	}
	gzclose(fd);
	BattleRoom_on_change_mod();
	MyBattle_update_mod_options();
	s_task_set_status = 1;
	SetEvent(s_event);
}

void
Sync_on_changed_map(const char *map_name)
{
	assert(GetCurrentThreadId() == GetWindowThreadProcessId(g_main_window, NULL));

	if (!_stricmp(s_current_map, map_name))
		return;

	for (int i=0; i<g_map_option_len; ++i)
		free(g_map_options[i].val);
	free(g_map_options);
	g_map_options = NULL;
	g_map_option_len = 0;

	char file_path[MAX_PATH];
	sprintf(file_path, "%lscache\\alphalobby\\%s.MapData", g_data_dir, map_name);

	gzFile fd = NULL;

	for (int i=0; i<g_map_len; ++i) {
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
		s_have_tried_download = 0;
		g_map_hash = 0;
		s_current_map[0] = 0;
		Minimap_set_bitmap(NULL, 0, 0, NULL, 0, 0, NULL);
		free(InterlockedExchangePointer(&s_map_to_save, _strdup(map_name)));
		SetEvent(s_event);
		MyBattle_update_mod_options();
		return;
	}

	strcpy(s_current_map, map_name);

	gzread(fd, &g_map_hash, sizeof g_map_hash);
	gzread(fd, &g_map_info, sizeof g_map_info);
	g_map_info.description = g_map_info.description;
	g_map_info.author = g_map_info.author;

	OptionList option_list = load_options(fd);
	g_map_options = option_list.xs;
	g_map_option_len = option_list.len;

	//Texture pixels:
	static uint16_t *pixels;
	if (!pixels)
		pixels = malloc(MAP_SIZE * sizeof *pixels);

	gzread(fd, pixels,MAP_SIZE * sizeof pixels[0]);

	//Heightmap pixels:
	static uint8_t *height_map_pixels;
	free(height_map_pixels);
	uint16_t h[2];
	gzread(fd, h, 4);
	height_map_pixels = malloc((size_t)(h[0] * h[1]));
	gzread(fd, height_map_pixels, (size_t)(h[0] *  h[1]));

	//Metalmap pixels:
	static uint8_t *metal_map_pixels;
	free(metal_map_pixels);
	uint16_t d[2];
	gzread(fd, d, 4);
	metal_map_pixels = malloc((size_t)(d[0] * d[1]));
	gzread(fd, metal_map_pixels, (size_t)(d[0] *  d[1]));

	Minimap_set_bitmap(pixels,
			d[0], d[1], metal_map_pixels,
			h[0], h[1], height_map_pixels);

	gzclose(fd);

	// task_set_minimap = 1;
	MyBattle_update_mod_options();
	s_task_set_status = 1;
	SetEvent(s_event);
}

void
Sync_reload(void)
{
	g_mod_hash = 0;
	s_current_mod[0] = 0;
	g_map_hash = 0;
	s_current_map[0] = 0;
	s_task_reload = 1;
	SetEvent(s_event);
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
				len = (size_t)sprintf(aiName, "%s", GetInfoValue(j));
			else if (!strcmp(GetInfoKey(j), "version"))
				sprintf(aiName + len, "|%s", GetInfoValue(j));
		func(aiName, arg);
	}
}

int
Sync_ai_option_len(const char *name)
{
	const char *name_len = strchr(name, '|');
	for (int imax=GetSkirmishAICount(), i=0; i<imax; ++i) {
		for (int jmax = GetSkirmishAIInfoCount(i), j=0; j<jmax; ++j) {
			if ((!strcmp("short_name", GetInfoKey(j))
				&& strncmp(name, GetInfoValue(j), (size_t)(name_len - name)))
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
		handle = FindFilesVFS(handle, buf, sizeof buf);
		SendMessageA(list_view_window, LVM_INSERTITEMA, 0, (intptr_t)&(LVITEMA){LVIF_TEXT, 0, .pszText = buf + sizeof "demos/" - 1});
	} while (handle);
}

void
Sync_cleanup(void)
{
	if (s_current_map[0])
		Settings_save_str("last_map", s_current_map);
	if (s_current_mod[0])
		Settings_save_str("last_mod", s_current_mod);
}
