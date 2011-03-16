#pragma once
#include <inttypes.h>
#include <stdbool.h>
#define MAP_DETAIL 2
#define MOD_OPTION_FLAG  0x8000
#define MAP_OPTION_FLAG  0x4000
#define STARTPOS_FLAG    0x2000
#define DOWNLOAD_MOD     1
#define DOWNLOAD_MAP     2
#define RELOAD_MAPS_MODS 3

#include "unitsync.h"

typedef enum OptionType{
  opt_error = 0, opt_bool = 1, opt_list = 2, opt_number = 3,
  opt_string = 4, opt_section = 5
} OptionType;

typedef struct Option2 {
	OptionType type;
	char *key, *name, *val;
	size_t nbListItems;
	struct OptionListItem {
		char *name, *key;
	}listItems[0];
	char extraData[1024];
}Option2;

uint32_t GetMapHash(const char *mapName);
uint32_t GetModHash(const char *modName);
void RedrawMinimapBoxes(void);
void ReloadMapsAndMod(void);
void ChangeMod(const char *modName);
void ChangeMap(const char *mapName);
void ChangeOption(uint16_t);
void SetScriptTags(char *);
const char * _GetSpringVersion(void);
void ForEachModName(void (*)(const char *));
void ForEachMapName(void (*)(const char *));
void ForEachAiName(void (*func)(const char *, void *), void *arg);
int UnitSync_GetSkirmishAIOptionCount(const char *name);
void UnitSync_GetOptions(int len; Option2 options[len], int len);
void CALLBACK UnitSync_AddReplaysToListView(HWND listViewWindow);
void UnitSync_Cleanup(void);
void Sync_Init(void);

#define RUN_IN_SYNC_THREAD(_func, _param) {\
	extern HANDLE gSyncThread;\
	QueueUserAPC((void *)(_func), gSyncThread, (ULONG_PTR)(_param));\
}
	
