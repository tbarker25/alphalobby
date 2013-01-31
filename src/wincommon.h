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

#ifndef WINCOMMON_H
#define WINCOMMON_H

typedef struct DialogItem {
	const wchar_t *class, *name;
	DWORD style, exStyle;
	// uint8_t flags;
} DialogItem;

HWND CreateDlgItem(HWND parent, const DialogItem *item, int dlgID);
void CreateDlgItems(HWND parent, const DialogItem items[], size_t n);

#define CreateWindowExW MyCreateWindowExW
HWND MyCreateWindowExW(DWORD dwExStyle, LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle,
		int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
		
		
int MyGetWindowTextA(HWND, char *str, int strLen);
#define GetWindowTextA MyGetWindowTextA
#define GetDlgItemTextA(_win, _id, _str, _len) MyGetWindowTextA(GetDlgItem((_win), (_id)), (_str), (_len))

//returns statically allocated memory (so only use in main thread, valid only until next call to MyGetWindowTextA2)
char * MyGetWindowTextA2(HWND window);
#define GetWindowTextA2 MyGetWindowTextA2
#define GetDlgItemTextA2(_win, _id) MyGetWindowTextA2(GetDlgItem((_win), (_id)))

#endif /* end of include guard: WINCOMMON_H */
