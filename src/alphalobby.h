#pragma once

enum {
	WM_POLL_SERVER = WM_APP + 0x0100,
	WM_DESTROY_WINDOW,
	WM_EXEC_FUNC,
	WM_MAKE_MESSAGEBOX,
	WM_CREATE_DLG_ITEM,
};

#define DLG_PROGRESS_BAR 0x100
#define DLG_PROGRESS_BUTTON 0x200

extern HWND gMainWindow;

void UpdateStatusBar(void);
void SetStatus(const wchar_t *text);
void Ring(void);

void FocusTab(HWND window);
void AddTab(HWND window);

void RemoveTab(HWND window);
void MainWindow_ChangeConnect(int isNowConnected);
int GetTabIndex(HWND window);

#define ExecuteInMainThread(_func)\
	SendMessage(gMainWindow, WM_EXEC_FUNC, (WPARAM)(_func), (WPARAM)0)	

void MyMessageBox(const char *caption, const char *text);