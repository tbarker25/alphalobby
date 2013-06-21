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
#include <stdbool.h>

#include <windows.h>
#include <commctrl.h>

#include "chatbox.h"
#include "tasserver.h"
#include "common.h"
#include "layoutmetrics.h"
#include "resource.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

static void activate(int item_index);
static BOOL CALLBACK channel_list_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
static void on_init(HWND window);

static HWND g_channel_list;

static void
activate(int item_index)
{
	wchar_t name[MAX_NAME_LENGTH_NUL];
	ListView_GetItemText(g_channel_list, item_index, 0, name, LENGTH(name));
	/* Chat_join_channel(utf16to8(name), 1); */
}

static void
on_init(HWND window)
{
	RECT rect;
	LVCOLUMN column_info;

	g_channel_list = GetDlgItem(window, IDC_USERLIST_LIST);
	GetClientRect(g_channel_list, &rect);
	column_info .mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH;

	column_info.pszText = L"name";
	column_info.cx = MAP_X(50);
	column_info.iSubItem = 0;
	ListView_InsertColumn(g_channel_list, 0, (LPARAM)&column_info);

	column_info.pszText = L"users";
	column_info.cx = MAP_X(20);
	column_info.iSubItem = 1;
	ListView_InsertColumn(g_channel_list, 1, (LPARAM)&column_info);

	column_info.pszText = L"description";
	column_info.cx = rect.right - MAP_X(70) - g_scroll_width;
	column_info.iSubItem = 2;
	ListView_InsertColumn(g_channel_list, 2, (LPARAM)&column_info);

	ListView_SetExtendedListViewStyleEx(g_channel_list,
			LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
}

static BOOL CALLBACK
channel_list_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg) {
	case WM_INITDIALOG:
		on_init(window);
		return 0;
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
			activate(ListView_GetNextItem(g_channel_list, -1, LVNI_SELECTED));
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
		g_channel_list = NULL;
		return 0;
	default:
		return 0;
	}
}

void
ChannelList_add_channel(const char *name, const char *user_len, const char *desc)
{
	int item_index;
	LVITEM item;

	item.mask = LVIF_TEXT;
	item.iSubItem = 0;
	item.pszText = utf8to16(name);
	item_index = ListView_InsertItem(g_channel_list, &item);

	assert(item_index != -1);

	ListView_SetItemText(g_channel_list, item_index, 1, utf8to16(user_len));
	ListView_SetItemText(g_channel_list, item_index, 2, utf8to16(desc));
}

void
ChannelList_show(void)
{
	/* DestroyWindow(GetParent(g_channel_list)); */
	assert(g_channel_list == NULL);
	CreateDialog(NULL, MAKEINTRESOURCE(IDD_USERLIST), NULL, channel_list_proc);
	TasServer_send_get_channels();
}
