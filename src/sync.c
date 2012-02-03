#include "gzip.h"
#include <assert.h>
#include <IL/il.h>

#include "wincommon.h"
#include "data.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
// #include "map.h"
#include "alphalobby.h"
#include "client_message.h"
#include "downloader.h"
#include "battleroom.h"
#include "dialogboxes.h"
#include "Richedit.h"
#include "sync.h"
#include "settings.h"
#include "imagelist.h"
#include <stdbool.h>
#include <Shlobj.h>

#define PLAIN_API_STRUCTURE
#define EXPORT2(type, name, args)\
	static type __stdcall (*name) args;
#include "unitsync_api.h"
#undef EXPORT2
#undef UNITSYNC_API_H

static const struct {
	void **proc;
	const char *name;
} x[] = {
#define EXPORT2(type, name, args)\
	{(void **)&name, #name},
#include "unitsync_api.h"
#undef EXPORT2
};

#define SYNCFILE_VERSION 0x01


static void createModFile(const char *modName);
static void createMapFile(const char *mapName);
static void setModInfo(void);
static void _setScriptTags(char *script);

//Shared between threads:
static uint8_t taskReload, /* taskSetMinimap, */ /* taskSetInfo, */ taskSetBattleStatus;
static HANDLE event;

//malloc new value, swap atomically, and free old value:
static const char *mapToSave, *modToSave;
static char *scriptToSet;

//Sync thread only:
static uint16_t *minimapPixels;
static char currentMod[MAX_TITLE];
static char currentMap[MAX_TITLE];

static void printLastError(wchar_t *title)
{
	wchar_t errorMessage[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	NULL, GetLastError(), 0, errorMessage, lengthof(errorMessage), NULL);
	MessageBox(NULL, errorMessage, title, 0);
}


__attribute__((noreturn))
static DWORD WINAPI syncThread (LPVOID lpParameter) 
{
	ilInit();
	
	HMODULE libUnitSync = LoadLibrary(L"unitsync.dll");
	if (!libUnitSync)
		printLastError(L"Could not load unitsync.dll");

	for (int i=0; i<lengthof(x); ++i) {
		*x[i].proc = GetProcAddress(libUnitSync, x[i].name);
		assert(*x[i].proc);
	}
	
	
	void *s; uint16_t i;
	event = CreateEvent(NULL, FALSE, 0, NULL);
	taskReload=1;
	while (1) {
		if (taskReload) {
			STARTCLOCK();
			taskReload = 0;
			Init(false, 0);
			ENDCLOCK();
			
			taskSetBattleStatus = 1;
			
			for (int i=0; i<gNbMods; ++i)
				free(gMods[i]);
			free(gMods);
			gNbMods = GetPrimaryModCount();
			gMods = malloc(gNbMods * sizeof(gMods[0]));
			for (int i=0; i<gNbMods; ++i)
				gMods[i] = strdup(GetPrimaryModName(i));
			
			for (int i=0; i<gNbMaps; ++i)
				free(gMaps[i]);
			free(gMaps);
			gNbMaps = GetMapCount();
			gMaps = malloc(gNbMaps * sizeof(gMaps[0]));
			for (int i=0; i<gNbMaps; ++i)
				gMaps[i] = strdup(GetMapName(i));
			
			SendMessage(gBattleRoomWindow, WM_RESYNC, 0, 0);
			if (gMyBattle) {
				ChangedMod(gMyBattle->modName);
				ChangedMap(gMyBattle->mapName);
			}
			ENDCLOCK();
		} else if ((s = (void *)__sync_fetch_and_and(&modToSave, NULL))) {
			STARTCLOCK();
			createModFile(s);
			free(s);
			ENDCLOCK();
		} else if ((s = (void *)__sync_fetch_and_and(&mapToSave, NULL))) {
			STARTCLOCK();
			createMapFile(s);
			free(s);
			ENDCLOCK();
		} else if (taskSetBattleStatus) {
			SetBattleStatus(&gMyUser, 0, 0);
			taskSetBattleStatus = 0;
		} else if ((s = __sync_fetch_and_and(&scriptToSet, NULL))) {
			STARTCLOCK();
			_setScriptTags(s);
			free(s);
			ENDCLOCK();
		} else {
			STARTCLOCK();
			ENDCLOCK();
			WaitForSingleObject(event, INFINITE);
		}
	};
}

uint32_t GetSyncStatus(void) {
	return (gMyBattle 
			&& gMapHash && (!gMyBattle->mapHash || gMapHash == gMyBattle->mapHash)
			&& gModHash && (!gBattleOptions.modHash || gModHash == gBattleOptions.modHash))
		? SYNCED : UNSYNCED;
}

static void setModInfo(void)
{
	char *buff = malloc(8192), *s=buff+sprintf(buff, R"({\rtf )");
	s += sprintf(s, R"(\b %s\par %s\b0\par)", *currentMod ? currentMod : gMyBattle ? gMyBattle->modName : "", *currentMap ? currentMap : gMyBattle ? gMyBattle->mapName : "");


	if (!gModHash || !gMapHash)
		s += sprintf(s, R"(\par)");
	if (!gModHash)
		s += sprintf(s, R"({\v :<)" STRINGIFY(DOWNLOAD_MOD) R"(}(download mod){\v >:}\par)");
	
	if (!gMapHash)
		s += sprintf(s, R"({\v :<)" STRINGIFY(DOWNLOAD_MAP) R"(}(download map){\v >:}\par)");
	
	if (!gModHash || !gMapHash)
		s += sprintf(s, R"({\v :<)" STRINGIFY(RELOAD_MAPS_MODS) R"(}(reload maps and mods){\v >:}\par)");
	
	if (gMapHash) {
		s += sprintf(s, R"(\par\i %s\i0\par )", gMapInfo.description);
		if (*gMapInfo.author)
			s += sprintf(s, R"(author: %s\par )", gMapInfo.author);
		s += sprintf(s, R"(Size: %dx%d     Gravity: %d\par Tidal: %d     Wind: %d-%d\par)", gMapInfo.width / 512, gMapInfo.height / 512, gMapInfo.tidalStrength, gMapInfo.gravity, gMapInfo.minWind, gMapInfo.maxWind);
	}
	
	const Option *options = gModOptions;
	int count = gNbModOptions;
	if (count)
		s += sprintf(s, R"(\par{\b Mod Options:}\par)");

	int flag = MOD_OPTION_FLAG;
	start:
	{
	

	int i=-1;
	goto entry;
	
	while (++i<count) {
		if (options[i].type != opt_section)
			continue;
		s += sprintf(s, R"(\par\b %s:\b0\par)", options[i].name);
		char *section = options[i].key;
		entry:
		for (int j=0; j<count; ++j) {
			if (options[j].type == opt_section)
				continue;
			if ((i != -1 ? &options[i] : NULL) != options[j].section)
				continue;
			s += sprintf(s, R"( %s: {\v :<%d })", options[j].name, flag | j);
			
			char *val = NULL;

			if (options)
				val = options[j].val;

			if (options[j].type == opt_number)
				s += sprintf(s, val);
			else if (options[j].type == opt_list) {
				for (int k=0; k < options[j].nbListItems; ++k) {
					if (!stricmp(val, options[j].listItems[k].key)) {
						s += sprintf(s, options[j].listItems[k].name);
					}
				}
			} else if (options[j].type == opt_bool)
				s += sprintf(s, val[0] != '0' ? "True" : "False");

			s += sprintf(s, R"({\v >:}\par)");
		}
	}

	if (flag == MOD_OPTION_FLAG && (count = gNbMapOptions)) {
		s += sprintf(s, R"(\par{\b Map Options:}\par)");
		flag = MAP_OPTION_FLAG;
		options = gMapOptions;
		goto start;
	}
	}
	sprintf(s, R"(\par})");
	PostMessage(gBattleRoomWindow, WM_SETMODDETAILS, 0, (LPARAM)buff);
}

void Sync_Init(void)
{
	CreateThread(NULL, 0, syncThread, NULL, 0, NULL);
}

void RedrawMinimapBoxes(void)
{
	// if (gBattleOptions.startPosType == STARTPOS_CHOOSE_INGAME) {
		// taskSetMinimap = 1;
		// SetEvent(event);
	// }
}

static void getOptionDef(int i, char *buff)
{
	switch (GetOptionType(i)) {
	case opt_bool:
		*(uint16_t *)buff = '0' + GetOptionBoolDef(i);
		break;
	case opt_number:
		sprintf(buff, "%g", GetOptionNumberDef(i));
		break;
	case opt_list:
		strcpy(buff, GetOptionListDef(i));
		break;
	default:
		// buff[0] = '\0';
		break;
	}
}

static void initOptions(size_t nbOptions, gzFile *fd)
{
	Option *options = calloc(10000, 1);
	char *s = (void *)&options[nbOptions];
	
	for (int i=0; i<nbOptions; ++i) {
		options[i].type = GetOptionType(i);
		assert(options[i].type != opt_error);
		options[i].key = s - (size_t)options;
		s += sprintf(s, "%s", GetOptionKey(i)) + 1;
		
		options[i].name = s - (size_t)options;
		s += sprintf(s, "%s", GetOptionName(i)) + 1;
		
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
					&& !stricmp(section, options[j].key + (size_t)options)) {
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

typedef struct OptionList {
	Option *xs;
	size_t len;
}OptionList;

static OptionList loadOptions(gzFile *fd)
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
		options[i].def += (size_t)options;
		options[i].val = strdup(options[i].def);
		if (options[i].section)
			options[i].section = &options[*(size_t *)&options[i].section - 1];
		
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

static void createMapFile(const char *mapName)
{
	STARTCLOCK();
	uint32_t mapHash = GetMapChecksumFromName(mapName);
	if (!mapHash)
		return;
	
	char filePath[MAX_PATH];
	sprintf(filePath, "%.*ls\\cache\\alphalobby\\%s.MapData", gWritableDataDirectoryLen - 1, gWritableDataDirectory, mapName);
	SHCreateDirectoryExW(NULL, GetWritablePath_unsafe(L"cache\\alphalobby"), NULL);
	gzFile fd = gzopen(filePath, "wb");
	gzputc(fd, SYNCFILE_VERSION);
	gzwrite(fd, &mapHash, sizeof(mapHash));
	struct _LargeMapInfo mapInfo = {.mapInfo = {.description = mapInfo.description, .author = mapInfo.author}};
	
	GetMapInfoEx(mapName, &mapInfo.mapInfo, 1);
	gzwrite(fd, &mapInfo, sizeof(mapInfo));
	initOptions(GetMapOptionCount(mapName), fd);
	uint16_t *minimapPixels2 = GetMinimap(mapName, MAP_DETAIL);
	assert(minimapPixels2);
	gzwrite(fd, minimapPixels2, MAP_SIZE*sizeof(minimapPixels2[0]));
	
	gzclose(fd);
	ExecuteInMainThreadParam(ChangedMap, mapName);
	puts(mapName);
}

static void createModFile(const char *modName)
{
	RemoveAllArchives();
	GetPrimaryModCount(); //todo investigate if we only need it on reinit
	int mod_index = GetPrimaryModIndex(modName);
	if (mod_index < 0)
		return;
	
	GetPrimaryModArchiveCount(mod_index);
	AddAllArchives(GetPrimaryModArchive(mod_index));
	
	char filePath[MAX_PATH];
	sprintf(filePath, "%.*ls\\cache\\alphalobby\\%s.ModData", gWritableDataDirectoryLen - 1, gWritableDataDirectory, modName);
	SHCreateDirectoryExW(NULL, GetWritablePath_unsafe(L"cache\\alphalobby"), NULL);
	gzFile fd = gzopen(filePath, "wb");
	gzputc(fd, SYNCFILE_VERSION);
	
	uint32_t modHash = GetPrimaryModChecksum(mod_index);
	gzwrite(fd, &modHash, sizeof(modHash));
	
	initOptions(GetModOptionCount(), fd);
	
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
		char vfsPath[128];
		int n = sprintf(vfsPath, "SidePics/%s.png", sideNames[i]);
		int fd = OpenFileVFS(vfsPath);
		if (!fd) {
			memcpy(&vfsPath[n - 3], (char[]){'b', 'm', 'p'}, 3);
			fd = OpenFileVFS(vfsPath);
		}
		if (!fd) {
			continue;
		}
		
		uint8_t buff[FileSizeVFS(fd)];
		ReadFileVFS(fd, buff, sizeof(buff));
		CloseFileVFS(fd);
		ilLoadL(IL_TYPE_UNKNOWN, buff, sizeof(buff));
		ilCopyPixels(0, 0, 0, 16, 16, 1, IL_BGRA, IL_UNSIGNED_BYTE, sidePics[i]);
		ILint format = ilGetInteger(IL_IMAGE_FORMAT);
		if (format == IL_RGB || format == IL_BGR) {
			for (int j=16 * 16 - 1; j>=0; --j)
				if (sidePics[i][j] == sidePics[i][0])
					sidePics[i][j] &= 0x00FFFFFF;
		}
	}
	gzwrite(fd, sidePics, sizeof(sidePics));
	
	gzclose(fd);
	ExecuteInMainThreadParam(ChangedMod, modName);
}

void ChangedMod(const char *modName)
{
	puts("Change Mod");
	if (!stricmp(currentMod, modName))
		return;

	char filePath[MAX_PATH];
	sprintf(filePath, "%.*ls\\cache\\alphalobby\\%s.ModData", gWritableDataDirectoryLen - 1, gWritableDataDirectory, modName);
	
	gzFile fd = gzopen(filePath, "rb");
	
	if (fd && gzgetc(fd) != SYNCFILE_VERSION) {
		gzclose(fd);
		remove(filePath);
		fd = 0;
	}
	if (!fd) {
		gNbSides = 0;
		gModHash = 0;
		currentMod[0] = 0;
		free(InterlockedExchangePointer(&modToSave, strdup(modName)));
		SetEvent(event);
		setModInfo();
		return;
	}
	strcpy(currentMod, modName);

	gzread(fd, &gModHash, sizeof(gModHash));
	OptionList modOptionList = loadOptions(fd);
	free(gModOptions);
	gModOptions = modOptionList.xs;
	gNbModOptions = modOptionList.len;
	
	gNbSides = gzgetc(fd);
	gzread(fd, gSideNames, gNbSides * 32);
	{
		uint32_t sidePics[gNbSides][16*16];
		gzread(fd, sidePics, sizeof(sidePics));
		for (int i=0; i<gNbSides; ++i) {
			HBITMAP bitmap = CreateBitmap(16, 16, 1, 32, sidePics[i]);
			ImageList_Replace(gIconList, ICONS_FIRST_SIDE + i, bitmap, NULL);
			DeleteObject(bitmap);
		}
	}
	
	gzclose(fd);
	
	PostMessage(gBattleRoomWindow, WM_CHANGEMOD, 0, 0);
	setModInfo();
	taskSetBattleStatus = 1;
	SetEvent(event);
}

void ChangedMap(const char *mapName)
{
	puts("changed map");
	if (!stricmp(currentMap, mapName))
		return;

	char filePath[MAX_PATH];
	sprintf(filePath, "%.*ls\\cache\\alphalobby\\%s.MapData", gWritableDataDirectoryLen - 1, gWritableDataDirectory, mapName);
	
	gzFile fd = gzopen(filePath, "rb");
	if (fd && gzgetc(fd) != SYNCFILE_VERSION) {
		gzclose(fd);
		remove(filePath);
		fd = 0;
	}
	if (!fd) {
		gMapHash = 0;
		currentMap[0] = 0;
		BattleRoom_ChangeMinimapBitmap(BLANK_MINIMAP);
		free(InterlockedExchangePointer(&mapToSave, strdup(mapName)));
		SetEvent(event);
		setModInfo();
		return;
	}
	
	strcpy(currentMap, mapName);
		
	gzread(fd, &gMapHash, sizeof(gMapHash));
	gzread(fd, &_gLargeMapInfo, sizeof(_gLargeMapInfo));
	gMapInfo.description = _gLargeMapInfo.description;
	gMapInfo.author = _gLargeMapInfo.author;
	
	OptionList optionList = loadOptions(fd);
	free(gMapOptions);
	gMapOptions = optionList.xs;
	gNbMapOptions = optionList.len;

	
	static uint16_t *pixels;
	if (!pixels)
		pixels = malloc(MAP_SIZE * sizeof(*pixels));
	
	gzread(fd, pixels,MAP_SIZE * sizeof(pixels[0]));
	BattleRoom_ChangeMinimapBitmap(pixels);

	gzclose(fd);
	
	// taskSetMinimap = 1;
	setModInfo();
	taskSetBattleStatus = 1;
	SetEvent(event);
}

void ReloadMapsAndMod(void)
{
	gModHash = 0;
	currentMod[0] = 0;
	gMapHash = 0;
	currentMap[0] = 0;
	taskReload = 1;
	SetEvent(event);
}

uint32_t GetMapHash(const char *mapName)
{
	return GetMapChecksumFromName(mapName);
}

uint32_t GetModHash(const char *modName)
{
	return GetPrimaryModChecksumFromName(modName);
}

static void _setScriptTags(char *script)
{
	for (char *key, *val; (key = strsplit(&script, "=")) && (val = strsplit(&script, "\t"));) {
		if (!_strnicmp(key, "game/startpostype", sizeof("game/startpostype") - 1)) {
			StartPosType startPosType = atoi(val);
			if (startPosType != gBattleOptions.startPosType)
				;// taskSetMinimap = 1;
			gBattleOptions.startPosType = startPosType;
			// PostMessage(gBattleRoomWindow, WM_MOVESTARTPOSITIONS, 0, 0);
			continue;
		} else if (!_strnicmp(key, "game/team", sizeof("game/team") - 1)) {
			int team = atoi(key + sizeof("game/team") - 1);
			char type = key[sizeof("game/team/startpos") + (team > 10)];
			((int *)&gBattleOptions.positions[team])[type != 'x'] = atoi(val);
			// PostMessage(gBattleRoomWindow, WM_MOVESTARTPOSITIONS, 0, 0);
		} else if (!_strnicmp(key, "game/modoptions/", sizeof("game/modoptions/") - 1)) {
			for (int i=0; i<gNbModOptions; ++i) {
				if (!strcmp(gModOptions[i].key, key + sizeof("game/modoptions/") - 1)) {
					free(gModOptions[i].val);
					gModOptions[i].val = strdup(val);
				}
			}
		} else if (!_strnicmp(key, "game/mapoptions/", sizeof("game/mapoptions/") - 1)) {
			for (int i=0; i<gNbMapOptions; ++i) {
				if (!strcmp(gMapOptions[i].key, key + sizeof("game/mapoptions/") - 1)) {
					free(gMapOptions[i].val);
					gMapOptions[i].val = strdup(val);
				}
			}
		} else {
			printf("unrecognized script key %s=%s\n", key, val);
		}
	}
	ExecuteInMainThread(setModInfo);
	ExecuteInMainThread(BattleRoom_StartPositionsChanged);
	// taskSetInfo = 1;
}

void SetScriptTags(char *script)
{
	if (gModOptions && gMapOptions)
		_setScriptTags(script);
	else {
		script = strdup(script);
		while (!__sync_bool_compare_and_swap(&scriptToSet, 0, script)) {
			char *oldScript = __sync_fetch_and_and(&scriptToSet, 0);
			char *s = strcat(strcpy(malloc(strlen(script) + strlen(oldScript) + 1), script), oldScript);
			free(script);
			script = s;
		}
	}
	SetEvent(event);
}

const char * _GetSpringVersion(void)
{
	return GetSpringVersion();
}

void ChangeOption(uint16_t iWithFlags)
{
	const char *key = NULL;
	char *val = NULL;
	char *path = NULL;
	int i = iWithFlags & (~MOD_OPTION_FLAG & ~MAP_OPTION_FLAG & ~STARTPOS_FLAG);
	
	Option *options;
	if (iWithFlags & MOD_OPTION_FLAG) {
		options = gModOptions;
		val = gModOptions[i].val;
		key = gModOptions[i].key;
		path = "modoptions/";
	} else if (iWithFlags & MAP_OPTION_FLAG) {
		options = gMapOptions;
		val = gMapOptions[i].val;
		key = gMapOptions[i].key;
		path = "mapoptions/";
	} else if (iWithFlags & STARTPOS_FLAG) {
		key = "startpostype";
		val = (char[2]){'0' + i};
		goto send;
	}
	
	switch (options[i].type) {
	case opt_bool: {
		val = (char [2]){val[0] ^ ('0' ^ '1')};
		break;
	} case opt_number: {
		char *tmp = alloca(128);
		tmp[0] = '\0';
		if (GetTextDlg(options[i].name, tmp, 128))
			return;
		val = tmp;
		} break;
	case opt_list: {
		HMENU menu = CreatePopupMenu();
		for (int j=0; j < options[i].nbListItems; ++j)
			AppendMenuA(menu, 0, j+1, options[i].listItems[j].name);
		SetLastError(0);
		POINT point;
		GetCursorPos(&point);
		void func(int *i) {
			*i = TrackPopupMenuEx(menu, TPM_RETURNCMD, point.x, point.y, gMainWindow, NULL);
		}
		int clicked;
		SendMessage(gMainWindow, WM_EXEC_FUNC, (WPARAM)func, (LPARAM)&clicked);
		if (!clicked)
			return;
		val = strcpy(alloca(128), options[i].listItems[clicked - 1].key);
		DestroyMenu(menu);
		} break;
	default:
		return;
	}


	send:
	if (gBattleOptions.hostType == HOST_SPADS) {
		SpadsMessageF("!bSet %s %s", key, val);
	} else if (gBattleOptions.hostType == HOST_SP) {
		Option * opt = &(iWithFlags & MOD_OPTION_FLAG ? gModOptions : gMapOptions)[i];
		free(opt->val);
		opt->val = strdup(val);
		setModInfo();
	} else if (gBattleOptions.hostType & HOST_FLAG) {
		SendToServer("!SETSCRIPTTAGS game/%s%s=%s", path ?: "", key, val);
	}
}

void ForEachAiName(void (*func)(const char *, void *), void *arg)
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

int UnitSync_GetSkirmishAIOptionCount(const char *name)
{
	const char *nameLen = strchr(name, '|');
	for (int imax=GetSkirmishAICount(), i=0; i<imax; ++i) {
		for (int jmax = GetSkirmishAIInfoCount(i), j=0; j<jmax; ++j) {
			if ((!strcmp("shortName", GetInfoKey(j))
					&& strncmp(name, GetInfoValue(j), nameLen - name))
				|| (!strcmp(GetInfoKey(j), "version")
					&& strcmp(nameLen+1, GetInfoValue(j))))
				goto nextAi;
		}
		return GetSkirmishAIOptionCount(i);
		nextAi:;
	}
	return -1;
}


void CALLBACK UnitSync_AddReplaysToListView(HWND listViewWindow)
{
	int handle = InitFindVFS("demos/*.sdf");
	if (handle < 0) //InitFileVFS has a different error return than FindFilesVFS
		return;
	do {
		char buffer[1024];
		handle = FindFilesVFS(handle, buffer, sizeof(buffer));
		SendMessageA(listViewWindow, LVM_INSERTITEMA, 0, (LPARAM)&(LVITEMA){LVIF_TEXT, 0, .pszText = buffer + sizeof("demos/") - 1});
	} while (handle);
}

void UnitSync_Cleanup(void)
{
	if (currentMap[0])
		SaveSetting("last_map", currentMap);
	if (currentMod[0])
		SaveSetting("last_mod", currentMod);
}


