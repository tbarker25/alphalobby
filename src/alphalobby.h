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

#ifndef ALPHALOBBY_H
#define ALPHALOBBY_H

#ifdef _WINDOWS_
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

#endif /* end of include guard: ALPHALOBBY_H */

