#pragma once
#define MAP_DETAIL 0
#define MAP_RESOLUTION (1024 >> MAP_DETAIL)
#define MAP_SIZE (MAP_RESOLUTION * MAP_RESOLUTION)
#define MOD_OPTION_FLAG  0x8000

struct Option;

uint32_t GetMapHash(const char *mapName);
uint32_t GetModHash(const char *modName);
void ReloadMapsAndMod(void);
void ChangedMod(const char *modName);
void ChangedMap(const char *mapName);
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
