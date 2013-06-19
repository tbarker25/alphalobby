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

#define ICONMASK_INGAME 1
#define ICONMASK_PASSWORD 2
#define ICONMASK_FULL 4

#define ICONMASK_USER 8
#define ICONMASK_SYNC 1
#define ICONMASK_AWAY 2
#define ICONMASK_IGNORE 4

typedef enum IconIndex {
	ICON_ALPHALOBBY = 0,

	ICON_MASK = 0x0F,

	ICON_CLOSED,
	ICON_OPEN,

	ICON_HOST,
	ICON_SPECTATOR,
	ICON_HOST_SPECTATOR,
	ICON_INGAME,

	ICON_FIRST_RANK,
	ICON_LAST_RANK = ICON_FIRST_RANK + 7,

	ICON_CONNECTING,
	ICON_BATTLELIST,
	ICON_REPLAY,
	ICON_CHAT,
	ICON_OPTIONS,
	ICON_DOWNLOADS,

	ICON_SPLIT_VERT,
	ICON_SPLIT_HORZ,
	ICON_SPLIT_CORNER1,
	ICON_SPLIT_CORNER2,
	ICON_SPLIT_RAND,

	ICON_FIRST_FLAG,
	ICON_FLAG_COUNT = 240,
	ICON_LAST_FLAG = ICON_FIRST_FLAG + ICON_FLAG_COUNT - 1,

	ICON_FIRST_SIDE,
	ICON_SIDE_COUNT = 16,
	ICON_LAST_SIDE = ICON_FIRST_SIDE + ICON_SIDE_COUNT - 1,

	ICON_FIRST_COLOR,
	ICON_COLOR_COUNT = 32,
	ICON_LAST_COLOR = ICON_FIRST_COLOR + ICON_COLOR_COUNT - 1,

	ICON_LAST = ICON_LAST_COLOR,

	ICON_READY = ICON_OPEN,
	ICON_UNREADY = ICON_CLOSED,

	ICON_ONLINE = ICON_OPEN,
	ICON_OFFLINE = ICON_CLOSED,
	ICON_BATTLEROOM = ICON_INGAME,
	ICON_HOSTBATTLE = ICON_HOST,
	ICON_SINGLEPLAYER = ICONMASK_INGAME,
} IconIndex;

union UserOrBot;

void  IconList_replace_icon        (IconIndex, HBITMAP);
HICON IconList_get_icon            (IconIndex);
int   IconList_get_user_color      (const union UserOrBot *);
void  IconList_set_window_image    (HWND, IconIndex);
void  IconList_enable_for_listview (HWND);
void  IconList_enable_for_toolbar  (HWND);

#endif /* end of include guard: ICONLIST_H */
