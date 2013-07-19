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

#include <windows.h>
#include <commctrl.h>

#include "chattab.h"
#include "channellist.h"
#include "chatbox.h"
#include "tasserver.h"
#include "common.h"
#include "layoutmetrics.h"
#include "resource.h"

#define LENGTH(x) (sizeof x / sizeof *x)

static void activate(int item_index);
static BOOL CALLBACK channel_list_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);
static void on_init(HWND window);

static HWND s_channel_list;

void
ChannelList_add_channel(const char *name, const char *user_len, const char *desc)
{
	int index;
	LVITEM item;

	item.mask = LVIF_TEXT;
	item.iSubItem = 0;
	item.pszText = utf8to16(name);
	index = ListView_InsertItem(s_channel_list, &item);

	assert(index != -1);

	ListView_SetItemText(s_channel_list, index, 1, utf8to16(user_len));
	ListView_SetItemText(s_channel_list, index, 2, utf8to16(desc));
}

void
ChannelList_show(void)
{
	assert(s_channel_list == NULL);
	CreateDialog(NULL, MAKEINTRESOURCE(IDD_USERLIST), NULL,
	    (DLGPROC)channel_list_proc);
	TasServer_send_get_channels();
}

static void
activate(int item_index)
{
	char name[MAX_NAME_LENGTH_NUL];
	LVITEMA info;

	info.iSubItem = 0;
	info.pszText = name;
	info.cchTextMax = sizeof name;
	SendMessage(s_channel_list, LVM_GETITEMTEXTA, (uintptr_t)item_index,
	    (intptr_t)&info);
	ChatTab_join_channel(name);
}

static void
on_init(HWND window)
{
	RECT rect;
	LVCOLUMN column_info;

	s_channel_list = GetDlgItem(window, IDC_USERLIST_LIST);
	GetClientRect(s_channel_list, &rect);
	column_info .mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH;

	column_info.pszText = (wchar_t *)L"name";
	column_info.cx = MAP_X(50);
	column_info.iSubItem = 0;
	ListView_InsertColumn(s_channel_list, 0, (uintptr_t)&column_info);

	column_info.pszText = (wchar_t *)L"users";
	column_info.cx = MAP_X(20);
	column_info.iSubItem = 1;
	ListView_InsertColumn(s_channel_list, 1, (uintptr_t)&column_info);

	column_info.pszText = (wchar_t *)L"description";
	column_info.cx = (int32_t)rect.right - (int32_t)MAP_X(70) - (int32_t)g_scroll_width;
	column_info.iSubItem = 2;
	ListView_InsertColumn(s_channel_list, 2, (uintptr_t)&column_info);

	ListView_SetExtendedListViewStyleEx(s_channel_list,
			LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
}

static BOOL CALLBACK
channel_list_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	switch (msg) {

	case WM_INITDIALOG:
		on_init(window);
		return 0;

	case WM_COMMAND:
		switch (w_param) {

		case MAKEWPARAM(IDOK, BN_CLICKED):
			activate(ListView_GetNextItem(s_channel_list, -1, LVNI_SELECTED));
			/* Fallthrough */

		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			DestroyWindow(window);
			return 0;

		default:
			return 0;
		}

	case WM_NOTIFY:
		if (((NMHDR *)l_param)->code == LVN_ITEMACTIVATE) {
			activate(((NMITEMACTIVATE *)l_param)->iItem);
			DestroyWindow(window);
		}
		return 0;

	case WM_DESTROY:
		s_channel_list = NULL;
		return 0;

	default:
		return 0;
	}
}
