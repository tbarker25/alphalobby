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

HWND CreateDlgItem(HWND parent, const DialogItem *item, int dlgID)
{
	HWND window = CreateWindowEx(item->exStyle, item->class, item->name, item->style | (parent ? WS_CHILD : 0),
		0, 0, 0, !item->class || wcscmp(item->class, WC_COMBOBOXEX) ? 0 : 1000,
		parent, (HMENU)dlgID, NULL, NULL);
	
	return window;
}

void CreateDlgItems(HWND parent, const DialogItem items[], size_t n)
{
	for (int i=0; i < n; ++i)
		CreateDlgItem(parent, &items[i], i);
}

#undef CreateWindowExW
HWND MyCreateWindowExW(DWORD dwExStyle, LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle,
		int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	HWND window = CreateWindowEx(dwExStyle, lpClassName, lpWindowName, dwStyle,
			x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	SendMessage(window, WM_SETFONT, (WPARAM)gFont, TRUE);
	return window;
}

int MyGetWindowTextA(HWND window, char *str, int strLen)
{
	wchar_t buff[MAX_SERVER_MESSAGE];
	GetWindowText(window, buff, LENGTH(buff));
	return WideCharToMultiByte(CP_UTF8, 0, buff, -1, str, strLen, NULL, NULL);
}

char * MyGetWindowTextA2(HWND window)
{
	static char buff[MAX_SERVER_MESSAGE];
	MyGetWindowTextA(window, buff, LENGTH(buff));
	return buff;
}


