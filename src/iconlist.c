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

#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>

#include <windows.h>
#include <Commctrl.h>

#include "battle.h"
#include "common.h"
#include "icons.h"
#include "iconlist.h"
#include "mybattle.h"
#include "user.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

#define ICON_BBP (4)
#define ICON_HEIGHT (16)
#define ICON_WIDTH (sizeof(icon_data) / ICON_BBP / ICON_HEIGHT)

static int get_player_color_index(const union UserOrBot *u);
static void __attribute__((constructor)) init(void);
static void set_icon_as_color(int index, uint32_t color);

static HIMAGELIST icon_list;

void
IconList_replace_icon(IconIndex index, HBITMAP bitmap)
{
	ImageList_Replace(icon_list, index, bitmap, NULL);
}

HICON
IconList_get_icon(IconIndex icon_index)
{
	return ImageList_GetIcon(icon_list, icon_index, 0);
}

static int
get_player_color_index(const union UserOrBot *u)
{
	static const UserOrBot *player_colors[ICON_LAST_COLOR - ICON_FIRST_COLOR + 1];

	if (!(u->battle_status & BS_MODE))
		return -1;

	for (size_t i=0; i <= LENGTH(player_colors); ++i) {
		if (player_colors[i] == u) {
			player_colors[i] = u;
			return i;
		}
	}

	for (size_t i=0; i <= LENGTH(player_colors); ++i) {
		for (size_t j = 0; j < g_my_battle->participant_len; ++j) {
			if (g_my_battle->users[j]->battle_status & BS_MODE
					&& g_my_battle->users[j] == player_colors[i])
				goto color_already_used;
		}
		player_colors[i] = u;
		return i;
color_already_used:;
	}
	return -1;
}

static void
set_icon_as_color(int index, uint32_t color)
{
	HDC dc;
	HDC bitmap_dc;
	HBITMAP bitmap;

	dc = GetDC(NULL);
	bitmap_dc = CreateCompatibleDC(dc);
	bitmap = CreateCompatibleBitmap(dc, 16, 16);
	ReleaseDC(NULL, dc);
	SelectObject(bitmap_dc, bitmap);

	for (size_t i=0; i<16*16; ++i)
		SetPixelV(bitmap_dc, i%16, i/16, color);

	DeleteDC(bitmap_dc);

	ImageList_Replace(icon_list, index, bitmap, NULL);
	DeleteObject(bitmap);
}

int
IconList_get_user_color(const union UserOrBot *u)
{
	int color_index;

	color_index = get_player_color_index(u);
	if (color_index < 0)
		return -1;

	set_icon_as_color(ICON_FIRST_COLOR + color_index, u->color);

	return ICON_FIRST_COLOR + color_index;
}

void
IconList_set_window_image(HWND window, IconIndex icon_index)
{
	HICON icon;

	icon = ImageList_GetIcon(icon_list, icon_index, 0);
	SendMessage(window, BM_SETIMAGE, IMAGE_ICON, (WPARAM)icon);
}

void
IconList_enable_for_listview(HWND window)
{
	SendMessage(window, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)icon_list);
}

void
IconList_enable_for_toolbar(HWND window)
{
	SendMessage(window, TB_SETIMAGELIST, 0, (LPARAM)icon_list);
}

static void __attribute__((constructor))
init(void)
{
	HBITMAP icons_bitmap;

	icon_list = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, ICON_LAST+1);
	icons_bitmap = CreateBitmap(ICON_WIDTH, ICON_HEIGHT, 1, ICON_BBP*8, icon_data);
	ImageList_Add(icon_list, icons_bitmap, NULL);
	DeleteObject(icons_bitmap);

	for (IconIndex i=1; i<=ICON_MASK; ++i)
		if (i & ~ USER_MASK)
			ImageList_SetOverlayImage(icon_list, i, i);
}
