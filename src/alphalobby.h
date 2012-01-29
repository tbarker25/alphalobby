#pragma once

#include "wincommon.h"

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

#define UpdateStatusBar()
#define SetStatus()

void Ring(void);

#define AddTab(x)
#define FocusTab(x)
#define RemoveTab(x)
#define GetTabIndex(x) -1

void MainWindow_ChangeConnect(int isNowConnected);

#define ExecuteInMainThread(_func)\
	SendMessage(gMainWindow, WM_EXEC_FUNC, (WPARAM)(_func), (WPARAM)0)	

void MyMessageBox(const char *caption, const char *text);

void SetCurrentTab(HWND newTab);
