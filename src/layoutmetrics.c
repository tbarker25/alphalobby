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

HFONT gFont;
uint16_t baseUnitX;
uint16_t baseUnitY;
uint32_t scrollWidth;

void InitializeSystemMetrics(void)
{
	NONCLIENTMETRICS info = {.cbSize = sizeof(NONCLIENTMETRICS)};

	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &info, 0);
	scrollWidth = info.iScrollWidth;
	gFont = CreateFontIndirect(&info.lfMessageFont);

	HDC dc = GetDC(NULL);
	SelectObject(dc, (HGDIOBJ)(HFONT)gFont);
	TEXTMETRIC tmp;
	GetTextMetrics(dc, &tmp);
	ReleaseDC(NULL, dc);
	baseUnitX = tmp.tmAveCharWidth;
	baseUnitY = tmp.tmHeight;
}

