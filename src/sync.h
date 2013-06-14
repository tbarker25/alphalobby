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

#define RUN_IN_SYNC_THREAD(_func, _param) {\
	extern HANDLE g_sync_thread;\
	QueueUserAPC((void *)(_func), g_sync_thread, (ULONG_PTR)(_param));\
}

struct Option;
typedef struct HWND__ *HWND;

void CALLBACK Sync_add_replays_to_listview(HWND list_view_window);
int           Sync_ai_option_len(const char *name);
void          Sync_cleanup(void);
void          Sync_for_each_ai(void (*func)(const char *, void *), void *arg);
uint32_t      Sync_is_synced(void) __attribute__((pure));
void          Sync_init(void);
uint32_t      Sync_map_hash(const char *map_name);
uint32_t      Sync_mod_hash(const char *mod_name);
void          Sync_on_changed_mod(const char *mod_name);
void          Sync_on_changed_map(const char *map_name);
void          Sync_reload(void);
const char *  Sync_spring_version(void);

#endif /* end of include guard: SYNC_H */
