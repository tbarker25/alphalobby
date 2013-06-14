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
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>
#include <commctrl.h>

#include "common.h"
#include "iconlist.h"
#include "layoutmetrics.h"
#include "resource.h"
#include "user.h"
#include "chat_window.h"
#include "chat.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

static void activate(int item_index);
static BOOL CALLBACK user_list_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
static void on_init(HWND window);

static HWND g_user_list;

static void
activate(int item_index)
{
	LVITEM item_info;
	item_info.mask = LVIF_PARAM;
	item_info.iItem = item_index;
	item_info.iSubItem = 2;
	ListView_GetItem(g_user_list, &item_info);
	ChatWindow_set_active_tab(Chat_get_private_window((User *)item_info.lParam));
}

static void
on_init(HWND window)
{
	RECT rect;
	LVCOLUMN column_info;

	DestroyWindow(GetParent(g_user_list));
	g_user_list = GetDlgItem(window, IDC_USERLIST_LIST);

	SetWindowLongPtr(g_user_list, GWL_STYLE, WS_VISIBLE | LVS_SHAREIMAGELISTS
			| LVS_SINGLESEL | LVS_REPORT | LVS_SORTASCENDING |
			LVS_NOCOLUMNHEADER);

	ListView_SetExtendedListViewStyle(g_user_list, LVS_EX_DOUBLEBUFFER | LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
	IconList_enable_for_listview(g_user_list);

	GetClientRect(g_user_list, &rect);
	column_info.mask = LVCF_SUBITEM | LVCF_WIDTH;

	column_info.cx = rect.right - ICON_SIZE - g_scroll_width;
	column_info.iSubItem = 0;
	ListView_InsertColumn(g_user_list, 0, (LPARAM)&column_info);

	column_info.cx = ICON_SIZE;
	column_info.iSubItem = 1;
	ListView_InsertColumn(g_user_list, 1, (LPARAM)&column_info);

	for (const User *u; (u = Users_get_next());) {
		if (!*u->name)
			continue;
		LVITEM item;
		item.mask = LVIF_PARAM | LVIF_TEXT | LVIF_STATE | LVIF_IMAGE;
		item.iItem = INT_MAX;
		item.iSubItem = 0;
		item.lParam = (LPARAM)u;

		wchar_t name[MAX_NAME_LENGTH * 2 + 4];
		_swprintf(name, strcmp(UNTAGGED_NAME(u->name), u->alias)
				? L"%hs (%hs)" : L"%hs",
				u->name, u->alias);
		item.pszText = name;

		item.iImage = u->ingame ? ICON_INGAME
			: u->battle ? ICONMASK_INGAME
			: -1;

		int icon_index = ICONMASK_USER;
		if (u->away)
		icon_index |= ICONMASK_AWAY;
		if (u->ignore)
		icon_index |= ICONMASK_IGNORE;
		item.state = INDEXTOOVERLAYMASK(icon_index);
		item.stateMask = LVIS_OVERLAYMASK;

		item.iItem = ListView_InsertItem(g_user_list, &item);

		item.mask = LVIF_IMAGE;
		item.iImage = ICON_FIRST_FLAG + u->country;
		item.iSubItem = 1;
		ListView_SetItem(g_user_list, &item);
	}
}

static BOOL CALLBACK
user_list_proc(HWND window, UINT msg, WPARAM w_param,
		LPARAM l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
		on_init(window);
		return 0;
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
			activate(ListView_GetNextItem(g_user_list, -1, LVNI_SELECTED));
			/* Fallthrough */
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			DestroyWindow(window);
			return 0;
		}
		return 0;
	case WM_NOTIFY:
		if (((LPNMHDR)l_param)->code == LVN_ITEMACTIVATE) {
			activate(((LPNMITEMACTIVATE)l_param)->iItem);
			DestroyWindow(window);
		}
		return 0;
	case WM_DESTROY:
		g_user_list = NULL;
		return 0;
	default:
		return 0;
	}
}

void
UserList_show(void)
{
	CreateDialog(NULL, MAKEINTRESOURCE(IDD_USERLIST), NULL, user_list_proc);
}
