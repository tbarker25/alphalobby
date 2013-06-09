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

HWND g_chat_window;

static HWND active_tab;
HWND tab_control;

enum DLG_ID {
	DLG_TAB,
	DLG_LAST = DLG_TAB,
};

static const DialogItem dialog_items[] = {
	[DLG_TAB] = {
		.class = WC_TABCONTROL,
		.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
	}
};

static void
resize_active_tab(void)
{
	RECT rect;
	GetClientRect(tab_control, &rect);
	TabCtrl_AdjustRect(tab_control, FALSE, &rect);
	MoveWindow(active_tab, rect.left, rect.top, rect.right - rect.left,
			rect.bottom - rect.top, 1);
}

int
get_tab_index(HWND tabItem)
{
	TCITEM info;
	info.mask = TCIF_PARAM;

	int nbTabs = TabCtrl_GetItemCount(tab_control);
	for (int i=0; i<nbTabs; ++i) {
		TabCtrl_GetItem(tab_control, i, &info);
		if ((HWND)info.lParam == tabItem)
			return i;
	}
	return -1;
}

int ChatWindow_add_tab(HWND tab)
{
	int item_index = get_tab_index(tab);
	if  (item_index >= 0)
		return item_index;

	wchar_t windowTitle[MAX_NAME_LENGTH_NUL];
	GetWindowText(tab, windowTitle, MAX_NAME_LENGTH_NUL);

	TCITEM info;
	info.mask = TCIF_TEXT | TCIF_PARAM;
	info.pszText = windowTitle;
	info.lParam = (LPARAM)tab;

	item_index = TabCtrl_InsertItem(tab_control, 1000, &info);
	if (item_index == 0) {
		active_tab = tab;
		ShowWindow(tab, 1);
		resize_active_tab();
	}

	return item_index;
}

void
ChatWindow_remove_tab(HWND tabItem)
{
	int index = get_tab_index(tabItem);
	TabCtrl_DeleteItem(tab_control, index);

	if (tabItem != active_tab)
		return;

	ShowWindow(tabItem, 0);

	if (index > 0)
		--index;

	TCITEM info;
	info.mask = TCIF_PARAM;
	TabCtrl_GetItem(tab_control, index, &info);

	active_tab = (HWND)info.lParam;
	ShowWindow(active_tab, 1);
	TabCtrl_SetCurSel(tab_control, index);
}

void
ChatWindow_set_active_tab(HWND tab)
{
	int item_index = ChatWindow_add_tab(tab);
	TabCtrl_SetCurSel(tab_control, item_index);
	ShowWindow(active_tab, 0);
	active_tab = tab;
	ShowWindow(active_tab, 1);
	resize_active_tab();
	MainWindow_set_active_tab(g_chat_window);
}

static LRESULT CALLBACK
chat_window_proc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
		g_chat_window = window;

		CreateDlgItems(window, dialog_items, DLG_LAST + 1);
		tab_control = GetDlgItem(window, DLG_TAB);
		return 0;
	case WM_SIZE:
		SetWindowPos(tab_control, HWND_BOTTOM, 0, 0,
				LOWORD(lParam), HIWORD(lParam), SWP_NOMOVE);
		resize_active_tab();
		break;
	case WM_CLOSE:
		ShowWindow(window, 0);
		return 0;
	case WM_NOTIFY: {
		NMHDR *info = (void *)lParam;
		if (info->hwndFrom == tab_control && info->code == TCN_SELCHANGE) {
			int tabIndex = TabCtrl_GetCurSel(info->hwndFrom);
			TCITEM info = {TCIF_PARAM};
			TabCtrl_GetItem(tab_control, tabIndex, &info);
			ChatWindow_set_active_tab((HWND)info.lParam);
			return 0;
		}
	}	break;
	}
	return DefWindowProc(window, msg, wParam, lParam);
}

static void __attribute__((constructor))
init (void)
{	WNDCLASS window_class = {
		.lpszClassName = WC_CHATWINDOW,
		.lpfnWndProc   = chat_window_proc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};
	RegisterClass(&window_class);
}
