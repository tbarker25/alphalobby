#pragma once

#include "wincommon.h"
#include "client.h"

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

#define AddTab(x) (x)
#define FocusTab(x) (ShowWindow(x, 1))
#define RemoveTab(x) (ShowWindow(x, 0))
#define GetTabIndex(x) -1

void MainWindow_ChangeConnect(enum ConnectionState);

#define ExecuteInMainThread(_func)\
	SendMessage(gMainWindow, WM_EXEC_FUNC, (WPARAM)(_func), (LPARAM)0)	

#define ExecuteInMainThreadParam(_func, _param)\
	SendMessage(gMainWindow, WM_EXEC_FUNC, (WPARAM)(_func), (LPARAM)_param)	

void MyMessageBox(const char *caption, const char *text);

void SetCurrentTab(HWND newTab);
void EnableBattleroomButton(void);
void DisableBattleroomButton(void);
