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

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include <windows.h>
#include <commctrl.h>

#include "battle.h"
#include "tasserver.h"
#include "common.h"
#include "iconlist.h"
#include "minimap.h"
#include "mybattle.h"
#include "sync.h"
#include "layoutmetrics.h"
#include "user.h"
#include "wincommon.h"

enum DialogId {
	DLG_SPLIT_FIRST,
	DLG_SPLIT_LAST = DLG_SPLIT_FIRST + SPLIT_LAST,

	DLG_SPLIT_SIZE,

	DLG_MAPMODE_MINIMAP,
	DLG_MAPMODE_METAL,
	DLG_MAPMODE_ELEVATION,
	DLG_CHANGE_MAP,

	DLG_LAST = DLG_CHANGE_MAP,
};

static void copy_heightmap       (uint32_t *dst, int width, int height);
static void copy_metalmap        (uint32_t *dst, int width, int height);
static void copy_minimap         (uint32_t *dst, int width, int height);
static void copy_normalmap       (uint32_t *dst, int width, int height);
static void copy_start_boxes     (uint32_t *dst, int width, int height);
static void copy_start_positions (uint32_t *dst, int width, int height);
static intptr_t CALLBACK minimap_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);
static void on_create            (HWND window);
static void on_draw              (HWND window);
static void on_size              (intptr_t l_param);
static void on_split_size_scroll (void);
static void set_split            (SplitType type, int size);
static void _init                (void);

static HWND s_minimap;
static enum MinimapType s_minimap_type = MINIMAP_NORMAL;
static const uint16_t *s_minimap_pixels;
static uint16_t s_metal_mapheight, s_metal_mapwidth;
static const uint8_t *s_metal_mappixels;
static uint16_t s_height_mapheight, s_height_mapwidth;
static const uint8_t *s_height_mappixels;

static const DialogItem DIALOG_ITEMS[] = {
	[DLG_MAPMODE_MINIMAP] = {
		.class = WC_BUTTON,
		.name = L"Minimap",
		.style = WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE,
	}, [DLG_MAPMODE_METAL] = {
		.class = WC_BUTTON,
		.name = L"Metal",
		.style = WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE,
	}, [DLG_MAPMODE_ELEVATION] = {
		.class = WC_BUTTON,
		.name = L"Elevation",
		.style = WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE,
	}, [DLG_CHANGE_MAP] = {
		.class = WC_BUTTON,
		.name = L"Change Map",
		.style = WS_VISIBLE | BS_PUSHBUTTON,

	}, [DLG_SPLIT_FIRST ... DLG_SPLIT_LAST] = {
		.class = WC_BUTTON,
		.style = WS_VISIBLE | BS_CHECKBOX | BS_PUSHLIKE | BS_ICON,

	}, [DLG_SPLIT_SIZE] = {
		.class = TRACKBAR_CLASS,
		.style = WS_VISIBLE | WS_CHILD,
	}
};

static void
set_split(SplitType type, int size)
{
	for (SplitType i=0; i<=SPLIT_LAST; ++i)
		SendDlgItemMessage(s_minimap, DLG_SPLIT_FIRST + i,
				BM_SETCHECK, i == type, 0);

	EnableWindow(GetDlgItem(s_minimap, DLG_SPLIT_SIZE),
			g_battle_options.start_pos_type == STARTPOS_CHOOSE_INGAME);

	SendDlgItemMessage(s_minimap, DLG_SPLIT_SIZE, TBM_SETPOS, 1, size);
}

void
Minimap_on_start_position_change(void)
{
	StartRect r1, r2;

	if (!g_battle_info_finished)
		return;

	Minimap_redraw();

	if (g_battle_options.start_pos_type == STARTPOS_RANDOM) {
		set_split(SPLIT_RAND, 0);
		return;
	}

	if (g_battle_options.start_pos_type != STARTPOS_CHOOSE_INGAME) {
		set_split(SPLIT_LAST + 1, 0);
		return;
	}

	if (g_battle_options.start_rects[0].left == 0
			&& (g_battle_options.start_rects[0].top == 0
				|| g_battle_options.start_rects[1].left != 0)) {
		r1 = g_battle_options.start_rects[0];
		r2 = g_battle_options.start_rects[1];
	} else {
		r1 = g_battle_options.start_rects[1];
		r2 = g_battle_options.start_rects[0];
	}

	if (r1.left == 0 && r1.top == 0 && r1.bottom == 200
			&& r2.top == 0 && r2.right == 200 && r2.bottom == 200
			&& r1.right == 200 - r2.left) {
		set_split(SPLIT_VERT, r1.right);
		return;
	}

	if (r1.left == 0 && r1.top == 0 && r1.right == 200
			&& r2.left == 0 && r2.right == 200 && r2.bottom == 200
			&& r1.bottom == 200 - r2.top) {
		set_split(SPLIT_HORZ, r1.bottom);
		return;
	}

	if (r1.left == 0 && r1.top == 0 && r2.right == 200 && r2.bottom == 200
			&& r1.right == 200 - r2.left
			&& r1.bottom == 200 - r2.top) {
		set_split(SPLIT_CORNERS1, (r1.right + r1.bottom) / 2);
		return;
	}

	if (r1.left == 0 && r1.bottom == 200 && r2.right == 200 && r2.top == 0
			&& r1.right == 200 - r2.left
			&& r1.top == 200 - r2.bottom) {
		set_split(SPLIT_CORNERS2, (r1.right + r2.bottom) / 2);
		return;
	}
}

static void
on_size(intptr_t l_param)
{
	HDWP dwp = BeginDeferWindowPos(DLG_LAST + 1);
	int w = LOWORD(l_param), h = HIWORD(l_param);

#define S 2
#define XS MAP_X(S)
#define YS MAP_Y(S)
#define YH MAP_Y(14)

#define MOVE_ID(id, x, y, cx, cy)\
	(DeferWindowPos(dwp, (GetDlgItem(s_minimap, (id))), NULL, (x), (y), (cx), (cy), 0))

	for (SplitType i=0; i<=SPLIT_LAST; ++i)
		MOVE_ID(DLG_SPLIT_FIRST + i,  (1 + i) * XS + i * YH, YS, MAP_Y(14), MAP_Y(14));
	MOVE_ID(DLG_SPLIT_SIZE,  6*XS + 5*YH, YS, w - 7*XS - 5*YH, YH);

#define TOP (h - MAP_Y(14 + S))
	MOVE_ID(DLG_MAPMODE_MINIMAP,    XS, TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_MAPMODE_METAL,      XS + MAP_X(50),  TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_MAPMODE_ELEVATION,  XS + MAP_X(100), TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_CHANGE_MAP,        w - 2*XS - MAP_X(60),   TOP, MAP_X(60), COMMANDBUTTON_Y);
#undef TOP
	EndDeferWindowPos(dwp);

	Minimap_redraw();
}


void Minimap_set_type(enum MinimapType type)
{
	s_minimap_type = type;
	Minimap_redraw();
}

void
Minimap_set_bitmap(const uint16_t *minimap_pixels,
		uint16_t metal_map_width, uint16_t metal_map_height, const uint8_t *metal_map_pixels,
		uint16_t height_map_width, uint16_t height_map_height, const uint8_t *height_map_pixels)
{
	s_minimap_pixels = minimap_pixels;

	s_metal_mapwidth = metal_map_width;
	s_metal_mapheight = metal_map_height;
	s_metal_mappixels = metal_map_pixels;

	s_height_mapwidth = height_map_width;
	s_height_mapheight = height_map_height;
	s_height_mappixels = height_map_pixels;

	Minimap_redraw();
}

void
Minimap_redraw(void)
{
	InvalidateRect(s_minimap, 0, 0);
}

static void
copy_heightmap(uint32_t *dst, int width, int height)
{
	for (int i=0; i<width * height; ++i) {
		int src_i;
		uint8_t height_p;

		src_i = i % width * s_height_mapwidth / width
			+ i / width * s_height_mapheight / height * s_height_mapwidth;
		height_p = s_height_mappixels[src_i];

		dst[i] = (uint32_t)(height_p | height_p << 8 | height_p << 16);
	}
}

static void
copy_metalmap(uint32_t *dst, int width, int height)
{
	for (int i=0; i<width * height; ++i) {
		int normal_i, metal_i;
		uint16_t normal_p;
		uint8_t metal_p;

		normal_i = i % width * MAP_RESOLUTION / width
			+ i / width * MAP_RESOLUTION / height * MAP_RESOLUTION;
		normal_p = s_minimap_pixels[normal_i];
		metal_i = i % width * s_metal_mapwidth / width
			+ i / width * s_metal_mapheight / height * s_metal_mapwidth;
		metal_p= s_metal_mappixels[metal_i];
		dst[i] = (uint32_t)((normal_p & 0x001B) << 1 | (normal_p & 0x700 ) << 3 | (normal_p & 0xE000) << 6);
		dst[i] |= (uint32_t)(metal_p>> 2 | metal_p<< 8);
	}
}

static void
copy_normalmap(uint32_t *dst, int width, int height)
{
	for (int i=0; i<width * height; ++i) {
		int src_i;
		uint16_t p;

		src_i = i % width * MAP_RESOLUTION / width
			+ i / width * MAP_RESOLUTION / height * MAP_RESOLUTION;
		p = s_minimap_pixels[src_i];
		dst[i] = (uint32_t)((p & 0x001F) << 3 | (p & 0x7E0 ) << 5 | (p & 0xF800) << 8);
	}
}

static void
copy_start_boxes(uint32_t *pixels, int width, int height)
{
	uint8_t ally;

	ally = !g_my_user.mode ? 255 : g_my_user.ally;

	for (uint8_t i=0; i<NUM_ALLIANCES; ++i) {
		uint16_t x_min, x_max, y_min, y_max;
		StartRect *start_rect;

		start_rect = &g_battle_options.start_rects[i];

		x_min = (uint16_t)(start_rect->left   * width  / START_RECT_MAX);
		x_max = (uint16_t)(start_rect->right  * width  / START_RECT_MAX);
		y_min = (uint16_t)(start_rect->top    * height / START_RECT_MAX);
		y_max = (uint16_t)(start_rect->bottom * height / START_RECT_MAX);

		if (i == ally) {
			for (uint16_t x=x_min; x<x_max; ++x) {
				for (uint16_t y=y_min; y<y_max; ++y) {
					if ((pixels[x+width*y] & 0x00FF00) >= (0x00FF00 - 0x003000))
						pixels[x+width*y] |= 0x00FF00;
					else
						pixels[x+width*y] += 0x003000;
				}
			}
		} else {
			for (uint16_t x=x_min; x<x_max; ++x) {
				for (uint16_t y=y_min; y<y_max; ++y) {
					if ((pixels[x+width*y] & 0xFF0000) >= (0xFF0000 - 0x300000))
						pixels[x+width*y] |= 0xFF0000;
					else
						pixels[x+width*y] += 0x300000;
				}
			}
		}

		for (uint16_t x=x_min; x<x_max; ++x) {
			pixels[x+width*y_min] = 0;
			pixels[x+width*(y_max-1)] = 0;
		}

		for (uint16_t y=y_min; y<y_max; ++y) {
			pixels[x_min+width*y] = 0;
			pixels[(x_max-1)+width*y] = 0;
		}
	}
}

static void
copy_start_positions(uint32_t *dst, int width, int height)
{
	uint8_t max;

	max = g_my_battle ? GetNumPlayers(g_my_battle) : 0;
	max = max < (uint8_t)g_map_info.pos_len ? max : (uint8_t)g_map_info.pos_len;

	for (uint8_t i=0; i<max; ++i) {
		uint16_t x_mid, y_mid;

		x_mid = (uint16_t)(g_map_info.positions[i].x * width / g_map_info.width);
		y_mid = (uint16_t)(g_map_info.positions[i].z * height / g_map_info.height);

		for (uint16_t x=(uint16_t)(x_mid-5); x<x_mid+5; ++x)
			for (uint16_t y=(uint16_t)(y_mid-5); y<y_mid+5; ++y)
				dst[x + width * y] = 0x00CC00;
	}
}

static void
copy_minimap(uint32_t *dst, int width, int height)
{
	switch (s_minimap_type) {

	case MINIMAP_HEIGHT:
		copy_heightmap(dst, width, height);
		break;

	case MINIMAP_METAL:
		copy_metalmap(dst, width, height);
		break;

	case MINIMAP_NORMAL:
		copy_normalmap(dst, width, height);
		break;
	}

	if (!g_my_battle)
		return;

	if (g_battle_options.start_pos_type == STARTPOS_CHOOSE_INGAME)
		copy_start_boxes(dst, width, height);
	else
		copy_start_positions(dst, width, height);
}

static void
on_draw(HWND window)
{
	PAINTSTRUCT ps;
	HBITMAP bitmap;
	HDC bitmap_context;
	int width, height;
	uint32_t *pixels;
	RECT rect;

	GetClientRect(window, &rect);
	BeginPaint(window, &ps);

	rect.top += MAP_Y(18);
	rect.bottom -= MAP_Y(36);

	FillRect(ps.hdc, &ps.rcPaint, (HBRUSH)(COLOR_BTNFACE + 1));

	if (!s_minimap_pixels)
		return;

	width = rect.right;
	height = rect.bottom;

	if (!g_map_info.width || !g_map_info.height || !width || !height)
		return;

	if (height * g_map_info.width > width * g_map_info.height) {
		height = width * g_map_info.height / g_map_info.width;

	} else {
		width = height * g_map_info.width / g_map_info.height;
	}

	if (!width || !height) {
		assert(0);
		return;
	}

	pixels = malloc((size_t)(width * height) * sizeof *pixels);
	copy_minimap(pixels, width, height);

	if (!pixels)
		return;

	bitmap = CreateBitmap(width, height, 1, 32, pixels);
	free(pixels);

	bitmap_context = CreateCompatibleDC(ps.hdc);
	SelectObject(bitmap_context, bitmap);

	BitBlt(ps.hdc, (rect.right - width) / 2,
	    rect.top + (rect.bottom - height) / 2,
	    rect.right, rect.bottom,
	    bitmap_context, 0, 0, SRCCOPY);

	DeleteDC(bitmap_context);

	DeleteObject(bitmap);
	EndPaint(window, &ps);
}

static intptr_t
on_command(uintptr_t w_param)
{
	HMENU menu;

	switch (w_param) {
	case MAKEWPARAM(DLG_CHANGE_MAP, BN_CLICKED): {
		POINT point;
		int map_index;

		menu = CreatePopupMenu();
		for (int i=0; i<g_map_len; ++i)
			AppendMenuA(menu, MF_CHECKED * !strcmp(g_my_battle->map_name,  g_maps[i]), (uintptr_t)i + 1, g_maps[i]);
		GetCursorPos(&point);
		map_index = TrackPopupMenuEx(menu, TPM_RETURNCMD, point.x, point.y, s_minimap, NULL);
		if (map_index > 0)
			TasServer_send_change_map(g_maps[map_index - 1]);
		DestroyMenu(menu);
		return 0;
	}
	case MAKEWPARAM(DLG_MAPMODE_MINIMAP, BN_CLICKED) ... MAKEWPARAM(DLG_MAPMODE_ELEVATION, BN_CLICKED):
		Minimap_set_type(LOWORD(w_param) - DLG_MAPMODE_MINIMAP);
		return 0;

	case MAKEWPARAM(DLG_SPLIT_FIRST, BN_CLICKED) ... MAKEWPARAM(DLG_SPLIT_LAST, BN_CLICKED):
		MyBattle_set_split(LOWORD(w_param) - DLG_SPLIT_FIRST, SendDlgItemMessage(s_minimap, DLG_SPLIT_SIZE, TBM_GETPOS, 0, 0));
		return 0;

	default:
		return 1;
	}
}

static void
on_split_size_scroll(void) {
	SplitType split_type;
	int split_size;

	/* find which split button is checked: */
	split_type = 0;
	while (!SendDlgItemMessage(s_minimap,
				DLG_SPLIT_FIRST + split_type,
				BM_GETCHECK, 0, 0)) {
		if (split_type > SPLIT_LAST) {
			assert(0);
			return;
		}
		++split_type;
	}

	split_size = SendDlgItemMessage(s_minimap, DLG_SPLIT_SIZE,
			TBM_GETPOS, 0, 0);
	MyBattle_set_split(split_type, split_size);
}

static void
on_create(HWND window)
{
	s_minimap = window;

	CreateDlgItems(window, DIALOG_ITEMS, DLG_LAST + 1);

	SendDlgItemMessage(window, DLG_SPLIT_SIZE, TBM_SETRANGE, 1,
			MAKELONG(0, 200));

	IconList_set_window_image(GetDlgItem(window, DLG_SPLIT_FIRST + SPLIT_VERT),
			ICON_SPLIT_VERT);
	IconList_set_window_image(GetDlgItem(window, DLG_SPLIT_FIRST + SPLIT_HORZ),
			ICON_SPLIT_HORZ);
	IconList_set_window_image(GetDlgItem(window, DLG_SPLIT_FIRST + SPLIT_CORNERS1),
			ICON_SPLIT_CORNER1);
	IconList_set_window_image(GetDlgItem(window, DLG_SPLIT_FIRST + SPLIT_CORNERS2),
			ICON_SPLIT_CORNER2);
	IconList_set_window_image(GetDlgItem(window, DLG_SPLIT_FIRST + SPLIT_RAND),
			ICON_SPLIT_RAND);

	SendDlgItemMessage(window, DLG_MAPMODE_MINIMAP, BM_SETCHECK, BST_CHECKED, 0);
}

static intptr_t CALLBACK
minimap_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	switch (msg) {

	case WM_CREATE:
		on_create(window);
		return 0;

	case WM_SIZE:
		on_size(l_param);

	case WM_PAINT:
		on_draw(window);
		return 0;

	case WM_COMMAND:
		return on_command(w_param);

	case WM_HSCROLL:
		if (GetDlgCtrlID((HWND)l_param) == DLG_SPLIT_SIZE
				&& w_param == SB_ENDSCROLL)
			on_split_size_scroll();
		return 0;

	default:
		return DefWindowProc(window, msg, w_param, l_param);
	}
}

static void __attribute__((constructor))
_init (void)
{
	WNDCLASSEX window_class = {
		.lpszClassName = WC_MINIMAP,
		.cbSize        = sizeof window_class,
		.lpfnWndProc   = (WNDPROC)minimap_proc,
		.hCursor       = LoadCursor(NULL, (wchar_t *)IDC_ARROW),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
	};

	RegisterClassEx(&window_class);
}
