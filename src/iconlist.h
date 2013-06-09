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

#ifndef ICONLIST_H
#define ICONLIST_H

#define INGAME_MASK 1
#define PW_MASK 2
#define FULL_MASK 4
#define USER_MASK 8
#define UNBS_SYNC 1
#define AWAY_MASK 2
#define IGNORE_MASK 4
enum ICONS {
	ICONS_MASK = 0x0F,

	ICONS_CLOSED,
	ICONS_OPEN,

	ICONS_HOST,
	ICONS_SPECTATOR,
	ICONS_HOST_SPECTATOR,
	ICONS_INGAME,

	ICONS_FIRST_RANK,
	ICONS_LAST_RANK = ICONS_FIRST_RANK + 7,

	ICONS_CONNECTING,
	ICONS_BATTLELIST,
	ICONS_REPLAY,
	ICONS_CHAT,
	ICONS_OPTIONS,
	ICONS_DOWNLOADS,

	ICONS_SPLIT_VERT,
	ICONS_SPLIT_HORZ,
	ICONS_SPLIT_CORNER1,
	ICONS_SPLIT_CORNER2,
	ICONS_SPLIT_RAND,

	ICONS_FIRST_FLAG,
	ICONS_FLAG_COUNT = 240,
	ICONS_LAST_FLAG = ICONS_FIRST_FLAG + ICONS_FLAG_COUNT - 1,

	ICONS_FIRST_SIDE,
	ICONS_SIDE_COUNT = 16,
	ICONS_LAST_SIDE = ICONS_FIRST_SIDE + ICONS_SIDE_COUNT - 1,

	ICONS_FIRST_COLOR,
	ICONS_COLOR_COUNT = 32,
	ICONS_LAST_COLOR = ICONS_FIRST_COLOR + ICONS_COLOR_COUNT - 1,

	ICONS_LAST = ICONS_LAST_COLOR,

	ICONS_READY = ICONS_OPEN,
	ICONS_UNREADY = ICONS_CLOSED,

	ICONS_ONLINE = ICONS_OPEN,
	ICONS_OFFLINE = ICONS_CLOSED,
	ICONS_BATTLEROOM = ICONS_INGAME,
	ICONS_HOSTBATTLE = ICONS_HOST,
	ICONS_SINGLEPLAYER = INGAME_MASK,
};

union UserOrBot;

int IconList_get_user_color(const union UserOrBot *);

#ifdef WINVER
extern HIMAGELIST g_icon_list;
#endif

#define EnableIconList(window)\
{\
	extern HIMAGELIST g_icon_list;\
	SendMessage(window, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)g_icon_list);\
}

#define EnableStateIconList(window)\
{\
	extern HIMAGELIST g_icon_list;\
	SendMessage(window, LVM_SETIMAGELIST, LVSIL_STATE, (LPARAM)g_icon_list);\
}

#endif /* end of include guard: ICONLIST_H */
