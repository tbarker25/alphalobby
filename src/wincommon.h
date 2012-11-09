#pragma once

typedef struct DialogItem {
	wchar_t *class, *name;
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
#define SendMsg(__window, __msg, __wparam, __lparam) \
	SendMessage((HWND)__window, (UINT)__msg, (WPARAM)__wparam, (LPARAM)__lparam)
