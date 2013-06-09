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

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>
#include <sys\stat.h>

#include <windows.h>
#include <Commctrl.h>

#include "battle.h"
#include "chat_window.h"
#include "common.h"
#include "mainwindow.h"
#include "mybattle.h"
#include "resource.h"
#include "user.h"
#include "wincommon.h"

HWND gChatWindow;

static HWND activeTab;
HWND tabControl;

enum DLG_ID {
	DLG_TAB,
	DLG_LAST = DLG_TAB,
};

static const DialogItem dialogItems[] = {
	[DLG_TAB] = {
		.class = WC_TABCONTROL,
		.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
	}
};


static void resizeActiveTab(void)
{
	RECT rect;
	GetClientRect(tabControl, &rect);
	TabCtrl_AdjustRect(tabControl, FALSE, &rect);
	MoveWindow(activeTab, rect.left, rect.top, rect.right - rect.left,
			rect.bottom - rect.top, 1);
}

int getTabIndex(HWND tabItem)
{
	TCITEM info;
	info.mask = TCIF_PARAM;

	int nbTabs = TabCtrl_GetItemCount(tabControl);
	for (int i=0; i<nbTabs; ++i) {
		TabCtrl_GetItem(tabControl, i, &info);
		if ((HWND)info.lParam == tabItem)
			return i;
	}
	return -1;
}

int ChatWindow_AddTab(HWND tab)
{
	int itemIndex = getTabIndex(tab);
	if  (itemIndex >= 0)
		return itemIndex;

	wchar_t windowTitle[MAX_NAME_LENGTH_NUL];
	GetWindowText(tab, windowTitle, MAX_NAME_LENGTH_NUL);

	TCITEM info;
	info.mask = TCIF_TEXT | TCIF_PARAM;
	info.pszText = windowTitle;
	info.lParam = (LPARAM)tab;

	itemIndex = TabCtrl_InsertItem(tabControl, 1000, &info);
	if (itemIndex == 0) {
		activeTab = tab;
		ShowWindow(tab, 1);
		resizeActiveTab();
	}

	return itemIndex;
}

void ChatWindow_RemoveTab(HWND tabItem)
{
	int index = getTabIndex(tabItem);
	TabCtrl_DeleteItem(tabControl, index);

	if (tabItem != activeTab)
		return;

	ShowWindow(tabItem, 0);

	if (index > 0)
		--index;

	TCITEM info;
	info.mask = TCIF_PARAM;
	TabCtrl_GetItem(tabControl, index, &info);

	activeTab = (HWND)info.lParam;
	ShowWindow(activeTab, 1);
	TabCtrl_SetCurSel(tabControl, index);
}

void ChatWindow_SetActiveTab(HWND tab)
{
	int itemIndex = ChatWindow_AddTab(tab);
	TabCtrl_SetCurSel(tabControl, itemIndex);
	ShowWindow(activeTab, 0);
	activeTab = tab;
	ShowWindow(activeTab, 1);
	resizeActiveTab();
	MainWindow_SetActiveTab(gChatWindow);
}

static LRESULT CALLBACK chatWindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
		gChatWindow = window;

		CreateDlgItems(window, dialogItems, DLG_LAST + 1);
		tabControl = GetDlgItem(window, DLG_TAB);
		return 0;
	case WM_SIZE:
		SetWindowPos(tabControl, HWND_BOTTOM, 0, 0,
				LOWORD(lParam), HIWORD(lParam), SWP_NOMOVE);
		resizeActiveTab();
		break;
	case WM_CLOSE:
		ShowWindow(window, 0);
		return 0;
	case WM_NOTIFY: {
		NMHDR *info = (void *)lParam;
		if (info->hwndFrom == tabControl && info->code == TCN_SELCHANGE) {
			int tabIndex = TabCtrl_GetCurSel(info->hwndFrom);
			TCITEM info = {TCIF_PARAM};
			TabCtrl_GetItem(tabControl, tabIndex, &info);
			ChatWindow_SetActiveTab((HWND)info.lParam);
			return 0;
		}
	}	break;
	}
	return DefWindowProc(window, msg, wParam, lParam);
}

static void __attribute__((constructor)) init (void)
{	WNDCLASS windowClass = {
		.lpszClassName = WC_CHATWINDOW,
		.lpfnWndProc   = chatWindowProc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};
	RegisterClass(&windowClass);
}
