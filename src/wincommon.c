#include <windows.h>
#include <windowsx.h>
#include <oleacc.h>
#include <Commctrl.h>
#include <stdio.h>
#include <richedit.h>
#include <Commctrl.h>
#include <inttypes.h>

#include "common.h"
#include "countryCodes.h"
#include "layoutmetrics.h"
#include "settings.h"
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


