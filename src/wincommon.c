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
#include <Commctrl.h>

#include "common.h"
#include "layoutmetrics.h"
#include "wincommon.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

HWND
CreateDlgItem(HWND parent, const DialogItem *item, int dialog_id)
{
	HWND window = CreateWindowEx(item->ex_style, item->class, item->name, item->style | (parent ? WS_CHILD : 0),
		0, 0, 0, !item->class || wcscmp(item->class, WC_COMBOBOXEX) ? 0 : 1000,
		parent, (HMENU)dialog_id, NULL, NULL);

	return window;
}

void
CreateDlgItems(HWND parent, const DialogItem items[], size_t n)
{
	for (size_t i=0; i < n; ++i)
		CreateDlgItem(parent, &items[i], i);
}

#undef CreateWindowExW
HWND
MyCreateWindowExW(DWORD dwEx_style, LPCTSTR lp_class_name, LPCTSTR lp_window_name, DWORD dwStyle,
		int x, int y, int n_width, int n_height, HWND hWnd_parent, HMENU hMenu, HINSTANCE hInstance, LPVOID lp_param)
{
	HWND window = CreateWindowEx(dwEx_style, lp_class_name, lp_window_name, dwStyle,
			x, y, n_width, n_height, hWnd_parent, hMenu, hInstance, lp_param);
	SendMessage(window, WM_SETFONT, (WPARAM)g_font, TRUE);
	return window;
}

int
MyGetWindowTextA(HWND window, char *str, int str_len)
{
	wchar_t buf[MAX_SERVER_MESSAGE];
	GetWindowText(window, buf, LENGTH(buf));
	return WideCharToMultiByte(CP_UTF8, 0, buf, -1, str, str_len, NULL, NULL);
}

char *
MyGetWindowTextA2(HWND window)
{
	static char buf[MAX_SERVER_MESSAGE];
	MyGetWindowTextA(window, buf, LENGTH(buf));
	return buf;
}
