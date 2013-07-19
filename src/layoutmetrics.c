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
	NONCLIENTMETRICS non_client_metrics;
	HDC dc;
	TEXTMETRIC text_metric;

	non_client_metrics.cbSize = sizeof non_client_metrics;
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof
	    non_client_metrics, &non_client_metrics, 0);

	g_scroll_width = (uint32_t)non_client_metrics.iScrollWidth;
	g_font = CreateFontIndirect(&non_client_metrics.lfMessageFont);

	dc = GetDC(NULL);
	SelectObject(dc, (HGDIOBJ)(HFONT)g_font);
	GetTextMetrics(dc, &text_metric);
	ReleaseDC(NULL, dc);
	g_base_unit_x = (uint16_t)text_metric.tmAveCharWidth;
	g_base_unit_y = (uint16_t)text_metric.tmHeight;
}
