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
#include <stdio.h>

#include <windows.h>
#include <Commctrl.h>

#include "common.h"
#include "iconlist.h"
#include "layoutmetrics.h"
#include "resource.h"
#include "user.h"
#include "chat_window.h"
#include "chat.h"

static HWND userList;

#define LENGTH(x) (sizeof(x) / sizeof(*x))

static void
activate(int item_index)
{
	LVITEM itemInfo;
	itemInfo.mask = LVIF_PARAM;
	itemInfo.iItem = item_index;
	itemInfo.iSubItem = 2;
	ListView_GetItem(userList, &itemInfo);
	ChatWindow_set_active_tab(Chat_get_private_window((User *)itemInfo.lParam));
}

static void
onInit(HWND window)
{
	RECT rect;
	LVCOLUMN columnInfo;

	DestroyWindow(GetParent(userList));
	userList = GetDlgItem(window, IDC_USERLIST_LIST);

	SetWindowLongPtr(userList, GWL_STYLE, WS_VISIBLE | LVS_SHAREIMAGELISTS
			| LVS_SINGLESEL | LVS_REPORT | LVS_SORTASCENDING |
			LVS_NOCOLUMNHEADER);

	ListView_SetExtendedListViewStyle(userList, LVS_EX_DOUBLEBUFFER | LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
	EnableIconList(userList);

	GetClientRect(userList, &rect);
	columnInfo.mask = LVCF_SUBITEM | LVCF_WIDTH;

	columnInfo.cx = rect.right - ICON_SIZE - scroll_width;
	columnInfo.iSubItem = 0;
	ListView_InsertColumn(userList, 0, (LPARAM)&columnInfo);

	columnInfo.cx = ICON_SIZE;
	columnInfo.iSubItem = 1;
	ListView_InsertColumn(userList, 1, (LPARAM)&columnInfo);

	for (const User *u; (u = Users_get_next());) {
		if (!*u->name)
			continue;
		LVITEM item = {};
		item.mask = LVIF_PARAM | LVIF_TEXT | LVIF_STATE | LVIF_IMAGE;
		item.iItem = INT_MAX;
		item.iSubItem = 0;
		item.lParam = (LPARAM)u;

		wchar_t name[MAX_NAME_LENGTH * 2 + 4];
		_swprintf(name, strcmp(UNTAGGED_NAME(u->name), u->alias)
				? L"%hs (%hs)" : L"%hs",
				u->name, u->alias);
		item.pszText = name;

		item.iImage = u->client_status & CS_INGAME ? ICONS_INGAME
			: u->battle ? INGAME_MASK
			: -1;

		int icon_index = USER_MASK;
		if (u->client_status & CS_AWAY)
		icon_index |= AWAY_MASK;
		if (u->ignore)
		icon_index |= IGNORE_MASK;
		item.state = INDEXTOOVERLAYMASK(icon_index);
		item.stateMask = LVIS_OVERLAYMASK;

		item.iItem = ListView_InsertItem(userList, &item);

		item.mask = LVIF_IMAGE;
		item.iImage = ICONS_FIRST_FLAG + u->country;
		item.iSubItem = 1;
		ListView_SetItem(userList, &item);
	}
}

static BOOL CALLBACK
user_list_proc(HWND window, UINT msg, WPARAM wParam,
		LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		onInit(window);
		return 0;
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
			activate(ListView_GetNextItem(userList, -1, LVNI_SELECTED));
			/* Fallthrough */
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			DestroyWindow(window);
			return 0;
		}
		return 0;
	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code == LVN_ITEMACTIVATE) {
			activate(((LPNMITEMACTIVATE)lParam)->iItem);
			DestroyWindow(window);
		}
		return 0;
	case WM_DESTROY:
		userList = NULL;
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
