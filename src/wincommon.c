#include "wincommon.h"
#include "layoutmetrics.h"
#include "settings.h"
#include <stdio.h>
#include <richedit.h>
#include <Commctrl.h>


#include "countryCodes.h"

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

wchar_t *utf8to16(const char *str)
{
	static wchar_t wStr[MAX_SERVER_MESSAGE];
	MultiByteToWideChar(CP_UTF8, 0, str, -1, wStr, lengthof(wStr));
	return wStr;
}

char *utf16to8(const wchar_t *wStr)
{
	static char str[MAX_SERVER_MESSAGE];
	WideCharToMultiByte(CP_UTF8, 0, wStr, -1, str, lengthof(str), NULL, NULL);
	return str;
}

int MyGetWindowTextA(HWND window, char *str, int strLen)
{
	wchar_t buff[MAX_SERVER_MESSAGE];
	GetWindowText(window, buff, lengthof(buff));
	return WideCharToMultiByte(CP_UTF8, 0, buff, -1, str, strLen, NULL, NULL);
}

char * MyGetWindowTextA2(HWND window)
{
	static char buff[MAX_SERVER_MESSAGE];
	MyGetWindowTextA(window, buff, lengthof(buff));
	return buff;
}


