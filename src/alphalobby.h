#pragma once

#ifdef _WINDOWS_H
enum {
	WM_POLL_SERVER = WM_APP + 0x0100,
	WM_DESTROY_WINDOW,
	WM_EXECFUNC,
	WM_EXECFUNCPARAM,
	WM_MAKE_MESSAGEBOX,
	WM_CREATE_DLG_ITEM,
};

#define DLG_PROGRESS_BAR 0x100
#define DLG_PROGRESS_BUTTON 0x200

#define ExecuteInMainThreadAsync(_func)\
	SendMessage(gMainWindow, WM_EXECFUNC, (WPARAM)(_func), 0)	

#define ExecuteInMainThread(_func)\
	SendMessage(gMainWindow, WM_EXECFUNC, (WPARAM)(_func), 0)	

#define ExecuteInMainThreadParam(_func, _param)\
	SendMessage(gMainWindow, WM_EXECFUNCPARAM, (WPARAM)(_func), (LPARAM)_param)	

extern HWND gMainWindow;
void MainWindow_SetActiveTab(HWND newTab);

#endif

enum ConnectionState;
void MainWindow_ChangeConnect(enum ConnectionState);


void MyMessageBox(const char *caption, const char *text);

void MainWindow_EnableBattleroomButton(void);
void MainWindow_DisableBattleroomButton(void);
void Ring(void);
