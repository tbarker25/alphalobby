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

HIMAGELIST g_icon_list;

int IconList_get_user_color(const union UserOrBot *s)
{
	if (!(s->battle_status & BS_MODE))
		return -1;

	static const UserOrBot *colors[ICONS_LAST_COLOR - ICONS_FIRST_COLOR + 1];
	int i;
	for (i=0; i <= ICONS_LAST_COLOR - ICONS_FIRST_COLOR; ++i)
		if (colors[i] == s)
			goto doit;

	for (i=0; i <= ICONS_LAST_COLOR - ICONS_FIRST_COLOR; ++i) {
		FOR_EACH_PLAYER(p, g_my_battle)
			if (p == colors[i])
				goto end;
		doit:
		colors[i] = s;
		HDC dc = GetDC(NULL);
		HDC bitmap_dc = CreateCompatibleDC(dc);
		HBITMAP bitmap = CreateCompatibleBitmap(dc, 16, 16);
		ReleaseDC(NULL, dc);
		SelectObject(bitmap_dc, bitmap);
		for (int i=0; i<16*16; ++i)
			SetPixelV(bitmap_dc, i%16, i/16, s->color);
		DeleteDC(bitmap_dc);

		ImageList_Replace(g_icon_list, ICONS_FIRST_COLOR + i, bitmap, NULL);
		DeleteObject(bitmap);
		return ICONS_FIRST_COLOR + i;

		end:;
	}
	return -1;
}

#define ICONS_BBP (4)
#define ICONS_HEIGHT (16)
#define ICONS_WIDTH (sizeof(icon_data) / ICONS_BBP / ICONS_HEIGHT)

static void __attribute__((constructor))
init(void)
{
	g_icon_list = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, ICONS_LAST+1);

	HBITMAP icons_bitmap = CreateBitmap(ICONS_WIDTH, ICONS_HEIGHT, 1, ICONS_BBP*8, icon_data);
	ImageList_Add(g_icon_list, icons_bitmap, NULL);
	DeleteObject(icons_bitmap);

	for (int i=1; i<=ICONS_MASK; ++i)
		if (i & ~ USER_MASK)
			ImageList_SetOverlayImage(g_icon_list, i, i);
}
