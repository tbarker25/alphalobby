#include <stdbool.h>
#include <assert.h>

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


static void setMod(void);
static void setMap(void);
static void setModInfo(void);
static void setBitmap(void);
static void changeOption(int);
static void _setScriptTags(char *script);

//Init in syncThread, then only use in mainthread:
static wchar_t writableDataDirectory[MAX_PATH];
static size_t writableDataDirectoryLen;

//Shared between threads:
static uint16_t optionToChange; 
static uint8_t taskReload, taskSetMinimap, taskSetInfo;
static HANDLE event;

//malloc new value, swap atomically, and free old value:
static const char *mapToSet, *modToSet;
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
	HMODULE libUnitSync = LoadLibrary(L"unitsync.dll");
	if (!libUnitSync)
		printLastError(L"Could not load unitsync.dll");

	for (int i=0; i<lengthof(x); ++i) {
		*x[i].proc = GetProcAddress(libUnitSync, x[i].name);
		assert(*x[i].proc);
	}
	
	
	void *s; uint16_t i;
	if ((s = LoadSetting("last_map")))
		mapToSet = strdup(s);
	if ((s = LoadSetting("last_mod")))
		modToSet = strdup(s);

	STARTCLOCK();
	Init(false, 0);
	SendMessage(gBattleRoomWindow, WM_RESYNC, 0, 0);
	// writableDataDirectoryLen = MultiByteToWideChar(CP_UTF8, 0, GetWritableDataDirectory(), -1, writableDataDirectory, lengthof(writableDataDirectory)) - 1;
	ENDCLOCK();

	event = CreateEvent(NULL, FALSE, 0, NULL);
	while (1) {
		if (taskReload) {
			STARTCLOCK();
			taskReload = 0;
			Init(false, 0);
			gModHash = 0;
			currentMod[0] = 0;
			gMapHash = 0;
			currentMap[0] = 0;
			SendMessage(gBattleRoomWindow, WM_RESYNC, 0, 0);
			ENDCLOCK();
		} else if ((s = (void *)__sync_fetch_and_and(&modToSet, NULL))) {
			// printf("mod %s %s\n", s, currentMod);
			if (strcmp(currentMod, s)) {
				strcpy(currentMod, s);
				free(s);
				STARTCLOCK();
				RemoveAllArchives();
				setMod();
				taskSetInfo = 1;
				PostMessage(gBattleRoomWindow, WM_CHANGEMOD, 0, 0);
				ENDCLOCK();
			}
		} else if ((s = (void *)__sync_fetch_and_and(&mapToSet, NULL))) {
			STARTCLOCK();
			// printf("map %s %s\n", s, currentMap);
			strcpy(currentMap, s);
			free(s);
			setMap();
			taskSetMinimap = 1;
			taskSetInfo = 1;
			ENDCLOCK();
		} else if ((s = __sync_fetch_and_and(&scriptToSet, NULL))) {
			STARTCLOCK();
			_setScriptTags(s);
			free(s);
			taskSetInfo = 1;
			ENDCLOCK();
		} else if (taskSetInfo) {
			STARTCLOCK();
			setModInfo();
			taskSetInfo = 0;
			ENDCLOCK();
		} else if (!minimapPixels && currentMap[0]) {
			STARTCLOCK();
			minimapPixels = (void *)GetMinimap(currentMap, MAP_DETAIL);
			ENDCLOCK();
		} else if (taskSetMinimap) {
			STARTCLOCK();
			taskSetMinimap = 0;
			setBitmap();
			ENDCLOCK();
		} else if ((i = __sync_fetch_and_and(&optionToChange, NULL))) {
			STARTCLOCK();
			changeOption(i);
			ENDCLOCK();
		} else {
			STARTCLOCK();
			SetBattleStatus(gMyUser, UNSYNCED >> (gMyBattle && gMapHash == gMyBattle->mapHash && gModHash == gBattleOptions.modHash), SYNC_MASK);
			ENDCLOCK();
			WaitForSingleObject(event, INFINITE);
		}
	};
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
	
	int count = GetModOptionCount();
	if (count)
		s += sprintf(s, R"(\par{\b Mod Options:}\par)");

	int flag = MOD_OPTION_FLAG;
	start:
	{
	
	OptionType optionTypes[count];
	char optionSections[count][256];
	for (int i=0; i<count; ++i) {
		optionTypes[i] = GetOptionType(i);
		strcpy(optionSections[i], GetOptionSection(i));
	}

	int i=-1; char section[256] = "";
	goto entry;
	
	while (++i<count) {
		if (optionTypes[i] != opt_section)
			continue;
		s += sprintf(s, R"(\par\b %s:\b0\par)", GetOptionName(i));
		strcpy(section, GetOptionKey(i));
		entry:
		for (int j=0; j<count; ++j) {
			if (optionTypes[j] == opt_section || _stricmp(section, optionSections[j]))
				continue;
			s += sprintf(s, R"( %s: {\v :<%d })", GetOptionName(j), flag | j);
			
			char *val = NULL;
			
			const Option *options = flag == MOD_OPTION_FLAG ? gModOptions : gMapOptions;
			if (options)
				val = options[j].val;

			if (optionTypes[j] == opt_number)
				s += sprintf(s, val);
			else if (optionTypes[j] == opt_list) {
				for (int k=0, nbListItems=GetOptionListCount(j); k < nbListItems; ++k) {
					if (!strcmp(val, GetOptionListItemKey(j, k))) {
						s += sprintf(s, GetOptionListItemName(j, k));
					}
				}
			} else if (optionTypes[j] == opt_bool)
				s += sprintf(s, val[0] != '0' ? "True" : "False");

			s += sprintf(s, R"({\v >:}\par)");
		}
	}

	if (flag == MOD_OPTION_FLAG && (count = GetMapOptionCount(currentMap))) {
		s += sprintf(s, R"(\par{\b Map Options:}\par)");
		flag = MAP_OPTION_FLAG;
		goto start;
	}
	}
	sprintf(s, R"(\par})");
	PostMessage(gBattleRoomWindow, WM_SETMODDETAILS, 0, (LPARAM)buff);
}

void setBitmap(void)
{
	#define res (1024 >> MAP_DETAIL)

	if (!minimapPixels) {
		PostMessage(gBattleRoomWindow, WM_REDRAWMINIMAP, 0, (LPARAM)NULL);
		return;
	}

	uint32_t pixels32[res*res];
	
	for (int i=0; i<res*res; ++i)
		pixels32[i] = (minimapPixels[i] & 0x001F) << 3 | (minimapPixels[i] & 0x7E0 ) << 5 | (minimapPixels[i] & 0xF800) << 8;
	
	if (gBattleOptions.startPosType == STARTPOS_CHOOSE_INGAME && gMyUser) {
		for (int j=0; j<NUM_ALLIANCES; ++j) {
			size_t color = (gMyUser->battleStatus & MODE_MASK) && j == FROM_ALLY_MASK(gMyUser->battleStatus) ? 1 : 2;
			int xMin = gBattleOptions.startRects[j].left * res / START_RECT_MAX;
			int xMax = gBattleOptions.startRects[j].right * res / START_RECT_MAX;
			int yMin = gBattleOptions.startRects[j].top * res / START_RECT_MAX;
			int yMax = gBattleOptions.startRects[j].bottom * res / START_RECT_MAX;
			for (int x=xMin; x<xMax; ++x) {
				for (int y=yMin; y<yMax; ++y) {
					uint8_t *p = &((uint8_t *)&pixels32[x+res*y])[color];
					*p = *p < 0xFF - 50 ? *p + 50 : 0xFF;
				}
			}
		}
	}
	PostMessage(gBattleRoomWindow, WM_REDRAWMINIMAP, 0, (LPARAM)CreateBitmap(res, res, 1, 32, pixels32));
}
 


void Sync_Init(void)
{
	// wcscpy(writableDataDirectory, L"c:\\programs\\spring\\");
	CreateThread(NULL, 0, syncThread, NULL, 0, NULL);
}

void RedrawMinimapBoxes(void)
{
	if (gBattleOptions.startPosType == STARTPOS_CHOOSE_INGAME) {
		taskSetMinimap = 1;
		SetEvent(event);
	}
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

static void initOptions(size_t nbOptions, Option **oldOptions, size_t *oldNbOptions)
{
	Option *options = malloc(nbOptions * sizeof(*options));
	for (int i=0; i<nbOptions; ++i) {
		options[i].key = strdup(GetOptionKey(i));
		switch (GetOptionType(i)) {
		case opt_bool: {
			options[i].val = malloc(2);
			*(uint16_t *)options[i].val = '0' + GetOptionBoolDef(i);
			break;
		case opt_number:
			options[i].val = malloc(128); //TODO, fix size here
			sprintf(options[i].val, "%g", GetOptionNumberDef(i));
			break;
		case opt_list:
			options[i].val = strdup(GetOptionListDef(i));
			break;
		default:
			options[i].val = NULL;
			break;
		}
		}
		options[i].def = strdup(options[i].val);
	}
	
	size_t nbOptionsToFree;
	Option * optionsToFree;
	void swapOptions(LPARAM unused)
	{
		nbOptionsToFree = *oldNbOptions;
		*oldNbOptions = nbOptions;
		optionsToFree = *oldOptions;
		*oldOptions = options;
	}
	ExecuteInMainThread(swapOptions);
	
	for (int i=0; i<nbOptionsToFree; ++i) {
		free(optionsToFree[i].key);
		free(optionsToFree[i].val);
		free(optionsToFree[i].def);
	}
	free(optionsToFree);
}

static void setMod(void)
{
	GetPrimaryModCount();
	int mod_index = GetPrimaryModIndex(currentMod);
	if (mod_index < 0) {
		for (int i=0; i<16; ++i)
			*gSideNames[i] = 0;
		gModHash = 0;
		currentMod[0] = 0;
		return;
	}
	GetPrimaryModArchiveCount(mod_index);
	AddAllArchives(GetPrimaryModArchive(mod_index));
	
	for (int i=0, sideCount = GetSideCount(); i < 16; ++i) {
		if (i >= sideCount) {
			*gSideNames[i] = '\0';
			ReplaceIcon(ICONS_FIRST_SIDE + i, NULL);
			continue;
		}
		strcpy(gSideNames[i], GetSideName(i));
		
		char vfsPath[128];
		sprintf(vfsPath, R"(SidePics/%s.bmp)", GetSideName(i));
		int ini = OpenFileVFS(vfsPath);
		uint8_t buff[FileSizeVFS(ini)];
		ReadFileVFS(ini, buff, sizeof(buff));
		CloseFileVFS(ini);
		
		wchar_t tmpFileName[MAX_PATH];
		GetTempPath(MAX_PATH, tmpFileName);
		GetTempFileName(tmpFileName, NULL, 0, tmpFileName);
		wcscat(tmpFileName, L".bmp");
		FILE *fd = _wfopen(tmpFileName, L"wb");
		fwrite(buff, 1, sizeof(buff), fd);
		fclose(fd);
		
		ReplaceIcon(ICONS_FIRST_SIDE + i, tmpFileName);
		
		_wremove(tmpFileName);
	}
	
	gModHash = GetPrimaryModChecksum(mod_index);
	
	initOptions(GetModOptionCount(), &gModOptions, &gNbModOptions);
}

static void setMap(void)
{
	gMapHash = GetMapChecksumFromName(currentMap);
	minimapPixels = NULL;
	if (!gMapHash) {
		currentMap[0] = 0;
		return;
	}
	GetMapInfoEx(currentMap, &gMapInfo, 1);
	
	initOptions(GetMapOptionCount(currentMap), &gMapOptions, &gNbMapOptions);
	SendMessage(gBattleRoomWindow, WM_MOVESTARTPOSITIONS, 0, 0);
}

void ChangeMod(const char *modName)
{
	// printf("changing mod to %s\n", modName);
	free(InterlockedExchangePointer(&modToSet, strdup(modName)));
	SetEvent(event);
}

void ChangeMap(const char *mapName)
{
	// printf("changing map to %s\n", mapName);
	free(InterlockedExchangePointer(&mapToSet, strdup(mapName)));
	SetEvent(event);
}

void ReloadMapsAndMod(void)
{
	if (gMyBattle) {
		free(InterlockedExchangePointer(&modToSet, gMyBattle->modName ? strdup(gMyBattle->modName) : NULL));
		free(InterlockedExchangePointer(&mapToSet, gMyBattle->mapName ? strdup(gMyBattle->mapName) : NULL));
	}
	
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
				taskSetMinimap = 1;
			gBattleOptions.startPosType = startPosType;
			PostMessage(gBattleRoomWindow, WM_MOVESTARTPOSITIONS, 0, 0);
			continue;
		} else if (!_strnicmp(key, "game/team", sizeof("game/team") - 1)) {
			int team = atoi(key + sizeof("game/team") - 1);
			char type = key[sizeof("game/team/startpos") + (team > 10)];
			((int *)&gBattleOptions.positions[team])[type != 'x'] = atoi(val);
			PostMessage(gBattleRoomWindow, WM_MOVESTARTPOSITIONS, 0, 0);
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
	taskSetInfo = 1;
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

void ChangeOption(uint16_t i)
{
	optionToChange = i;
	SetEvent(event);
}

static void changeOption(int iWithFlags)
{
	const char *key = NULL;
	char *val = NULL;
	char *path = NULL;
	int i = iWithFlags & (~MOD_OPTION_FLAG & ~MAP_OPTION_FLAG & ~STARTPOS_FLAG);
	if (iWithFlags & MOD_OPTION_FLAG) {
		GetModOptionCount();
		val = gModOptions[i].val;
		key = gModOptions[i].key;
		path = "modoptions/";
	} else if (iWithFlags & MAP_OPTION_FLAG) {
		GetMapOptionCount(currentMap);
		val = gMapOptions[i].val;
		key = gMapOptions[i].key;
		path = "mapoptions/";
	} else if (iWithFlags & STARTPOS_FLAG) {
		key = "startpostype";
		val = (char[2]){'0' + i};
		goto send;
	}
	
	switch (GetOptionType(i)) {
	case opt_bool: {
		val = (char [2]){val[0] ^ ('0' ^ '1')};
		break;
	} case opt_number: {
		char *tmp = alloca(128);
		tmp[0] = '\0';
		if (GetTextDlg(GetOptionName(i), tmp, 128))
			return;
		val = tmp;
		} break;
	case opt_list: {
		HMENU menu = CreatePopupMenu();
		for (int j=0, nbListItems=GetOptionListCount(i); j < nbListItems; ++j)
			AppendMenuA(menu, 0, j+1, GetOptionListItemName(i, j));
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
		val = strcpy(alloca(128), GetOptionListItemKey(i, clicked - 1));
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
		taskSetInfo = 1;
		SetEvent(event);
	} else if (gBattleOptions.hostType & HOST_FLAG) {
		SendToServer("!SETSCRIPTTAGS game/%s%s=%s", path ?: "", key, val);
	}
}

void ForEachModName(void (*func)(const char *))
{
	for (int n=GetPrimaryModCount(), i=0; i<n; ++i)
		func(GetPrimaryModName(i));

}

void ForEachMapName(void (*func)(const char *))
{
	for (int n=GetMapCount(), i=0; i<n; ++i)
		func(GetMapName(i));
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

void UnitSync_GetOptions(int len; Option2 options[len], int len)
{
	for (int i=0; i<len; ++i) {
		options[i].type = GetOptionType(i);
		
		void *s = options[i].extraData;
		if (options[i].type == opt_list) {
			options[i].nbListItems=GetOptionListCount(i);
			s = &options[i].listItems[options[i].nbListItems];
			for (int j=0; j < options[i].nbListItems; ++j) {
				s = strpcpy((options[i].listItems[j].key=s), GetOptionListItemKey(i,j));
				s = strpcpy((options[i].listItems[j].name=s), GetOptionListItemName(i, j));
			}
		}
		s = strpcpy((options[i].key=s), GetOptionKey(i));
		s = strpcpy((options[i].name=s), GetOptionName(i));
		getOptionDef(i, s);
		if (*(char *)s)
			options[i].val = s;
	}
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


