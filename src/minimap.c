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

#include "battle.h"
#include "minimap.h"
#include "mybattle.h"
#include "common.h"
#include "sync.h"
#include "user.h"

static void _init (void);
static LRESULT CALLBACK minimap_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
static void on_draw(HWND window);

static void copy_heightmap       (uint32_t *dst, int width, int height);
static void copy_normalmap       (uint32_t *dst, int width, int height);
static void copy_metalmap        (uint32_t *dst, int width, int height);
static uint32_t * copy_minimap   (int width, int height);
static void copy_start_boxes     (uint32_t *pixels, int width, int height);
static void copy_start_positions (uint32_t *dst, int width, int height);

static HWND minimap;

static enum MinimapType minimapType = MINIMAP_NORMAL;
static const uint16_t *minimap_pixels;
static uint16_t metal_map_height, metal_map_width;
static const uint8_t *metal_map_pixels;
static uint16_t height_map_height, height_map_width;
static const uint8_t *height_map_pixels;

void Minimap_set_type(enum MinimapType type)
{
	minimapType = type;
	Minimap_redraw();
}

void
Minimap_set_bitmap(const uint16_t *_minimap_pixels,
		uint16_t _metal_map_width, uint16_t _metal_map_height, const uint8_t *_metal_map_pixels,
		uint16_t _height_map_width, uint16_t _height_map_height, const uint8_t *_height_map_pixels)
{
	minimap_pixels = _minimap_pixels;

	metal_map_width = _metal_map_width;
	metal_map_height = _metal_map_height;
	metal_map_pixels = _metal_map_pixels;

	height_map_width = _height_map_width;
	height_map_height = _height_map_height;
	height_map_pixels = _height_map_pixels;

	Minimap_redraw();
}

void
Minimap_redraw(void)
{
	InvalidateRect(minimap, 0, 0);
}

static void
copy_heightmap(uint32_t *dst, int width, int height)
{
	for (int i=0; i<width * height; ++i) {
		size_t src_i;
		uint8_t height_p;

		src_i = i % width * height_map_width / width
			+ i / width * height_map_height / height * height_map_width;
		height_p = height_map_pixels[src_i];

		dst[i] = height_p | height_p << 8 | height_p << 16;
	}
}

static void
copy_metalmap(uint32_t *dst, int width, int height)
{
	for (int i=0; i<width * height; ++i) {
		size_t normal_i, metal_i;
		uint16_t normal_p;
		uint8_t metal_p;

		normal_i = i % width * MAP_RESOLUTION / width
			+ i / width * MAP_RESOLUTION / height * MAP_RESOLUTION;
		normal_p = minimap_pixels[normal_i];
		metal_i = i % width * metal_map_width / width
			+ i / width * metal_map_height / height * metal_map_width;
		metal_p= metal_map_pixels[metal_i];
		dst[i] = (normal_p & 0x001B) << 1 | (normal_p & 0x700 ) << 3 | (normal_p & 0xE000) << 6;
		dst[i] |= metal_p>> 2 | metal_p<< 8;
	}
}

static void
copy_normalmap(uint32_t *dst, int width, int height)
{
	for (int i=0; i<width * height; ++i) {
		size_t src_i;
		uint16_t p;

		src_i = i % width * MAP_RESOLUTION / width
			+ i / width * MAP_RESOLUTION / height * MAP_RESOLUTION;
		p = minimap_pixels[src_i];
		dst[i] = (p & 0x001F) << 3 | (p & 0x7E0 ) << 5 | (p & 0xF800) << 8;
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

		x_min = start_rect->left   * width  / START_RECT_MAX;
		x_max = start_rect->right  * width  / START_RECT_MAX;
		y_min = start_rect->top    * height / START_RECT_MAX;
		y_max = start_rect->bottom * height / START_RECT_MAX;

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
	max = max < g_map_info.pos_len ? max : g_map_info.pos_len;

	for (uint8_t i=0; i<max; ++i) {
		uint16_t x_mid, y_mid;

		x_mid = g_map_info.positions[i].x * width / g_map_info.width;
		y_mid = g_map_info.positions[i].z * height / g_map_info.height;

		for (uint16_t x=x_mid-5; x<x_mid+5; ++x)
			for (uint16_t y=y_mid-5; y<y_mid+5; ++y)
				dst[x + width * y] = 0x00CC00;
	}
}

static uint32_t *
copy_minimap(int width, int height)
{
	uint32_t *ret;

	if (!minimap_pixels)
		return NULL;


	ret = malloc(width * height * sizeof(*ret));

	switch (minimapType) {

	case MINIMAP_HEIGHT:
		copy_heightmap(ret, width, height);
		break;

	case MINIMAP_METAL:
		copy_metalmap(ret, width, height);
		break;

	case MINIMAP_NORMAL:
		copy_normalmap(ret, width, height);
		break;
	}

	if (!g_my_battle)
		return ret;

	if (g_battle_options.start_pos_type == STARTPOS_CHOOSE_INGAME)
		copy_start_boxes(ret, width, height);
	else
		copy_start_positions(ret, width, height);

	return ret;
}

static void
on_draw(HWND window)
{
	PAINTSTRUCT ps;
	HDC drawing_context;
	HBITMAP bitmap;
	HDC bitmap_context;
	RECT rect;
	uint32_t *pixels;
	int width, height;

	GetClientRect(window, &rect);
	width = rect.right;
	height = rect.bottom;

	drawing_context = BeginPaint(window, &ps);
	FillRect(drawing_context, &rect, (HBRUSH) (COLOR_BTNFACE+1));

	if (!g_map_info.width || !g_map_info.height || !width || !height) {
		return;
	}


	if (height * g_map_info.width > width * g_map_info.height)
		height = width * g_map_info.height / g_map_info.width;
	else
		width = height * g_map_info.width / g_map_info.height;

	if (!width || !height) {
		assert(0);
		return;
	}

	pixels = copy_minimap(width, height);

	if (!pixels)
		return;

	bitmap = CreateBitmap(width, height, 1, 32, pixels);
	free(pixels);

	bitmap_context = CreateCompatibleDC(drawing_context);
	SelectObject(bitmap_context, bitmap);

	BitBlt(drawing_context,
			(rect.right - width) / 2,
			(rect.bottom - height) / 2,
			rect.right, rect.bottom,
			bitmap_context, 0, 0, SRCCOPY);

	DeleteObject(bitmap);
	EndPaint(window, &ps);
}

static LRESULT CALLBACK
minimap_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg) {

	case WM_CREATE:
		minimap = window;
		return 0;

	case WM_PAINT:
		on_draw(window);
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
		.cbSize        = sizeof(WNDCLASSEX),
		.lpfnWndProc   = minimap_proc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};

	RegisterClassEx(&window_class);
}
