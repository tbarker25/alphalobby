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

#ifndef SYNC_H
#define SYNC_H

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
#ifdef _WINDOWS_
void CALLBACK UnitSync_AddReplaysToListView(HWND listViewWindow);
#endif
void UnitSync_Cleanup(void);
void Sync_Init(void);

#define RUN_IN_SYNC_THREAD(_func, _param) {\
	extern HANDLE gSyncThread;\
	QueueUserAPC((void *)(_func), gSyncThread, (ULONG_PTR)(_param));\
}

#endif /* end of include guard: SYNC_H */
