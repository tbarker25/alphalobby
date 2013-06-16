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

#include <windows.h>

static void _init(void);

HFONT g_font;
uint16_t g_base_unit_x;
uint16_t g_base_unit_y;
uint32_t g_scroll_width;

static void __attribute__((constructor))
_init(void)
{
	NONCLIENTMETRICS info = {.cbSize = sizeof(NONCLIENTMETRICS)};

	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &info, 0);
	g_scroll_width = info.iScrollWidth;
	g_font = CreateFontIndirect(&info.lfMessageFont);

	HDC dc = GetDC(NULL);
	SelectObject(dc, (HGDIOBJ)(HFONT)g_font);
	TEXTMETRIC tmp;
	GetTextMetrics(dc, &tmp);
	ReleaseDC(NULL, dc);
	g_base_unit_x = tmp.tmAveCharWidth;
	g_base_unit_y = tmp.tmHeight;
}
