#pragma once
#include <inttypes.h>
#include <stdbool.h>
#define MAP_DETAIL 0
#define MOD_OPTION_FLAG  0x8000
#define MAP_OPTION_FLAG  0x4000
#define STARTPOS_FLAG    0x2000
#define DOWNLOAD_MOD     1
#define DOWNLOAD_MAP     2
#define RELOAD_MAPS_MODS 3

#include "unitsync.h"
#include "data.h"


uint32_t GetMapHash(const char *mapName);
uint32_t GetModHash(const char *modName);
void RedrawMinimapBoxes(void);
void ReloadMapsAndMod(void);
void ChangeMod(const char *modName);
void ChangedMap(const char *mapName);
void ChangeOption(uint16_t);
void SetScriptTags(char *);
const char * _GetSpringVersion(void);
uint32_t GetSyncStatus(void);
void ForEachAiName(void (*func)(const char *, void *), void *arg);
int UnitSync_GetSkirmishAIOptionCount(const char *name);
void CALLBACK UnitSync_AddReplaysToListView(HWND listViewWindow);
void UnitSync_Cleanup(void);
void Sync_Init(void);

#define RUN_IN_SYNC_THREAD(_func, _param) {\
	extern HANDLE gSyncThread;\
	QueueUserAPC((void *)(_func), gSyncThread, (ULONG_PTR)(_param));\
}
	
