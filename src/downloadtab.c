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

#include <inttypes.h>

#include <windows.h>
#include <commctrl.h>

#include "dialogs/rapiddialog.h"
#include "downloadtab.h"
#include "layoutmetrics.h"
#include "resource.h"
#include "wincommon.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

enum DialogId {
	DLG_LIST,
	DLG_RAPID,
	DLG_LAST = DLG_RAPID,
};

static void _init (void);
static LRESULT CALLBACK download_list_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
static void resize_columns(void);

static HWND g_download_list;
static const wchar_t *const COLUMN_TITLES[] = {L"Name", L"Status"};

static const DialogItem DIALOG_ITEMS[] = {
	[DLG_LIST] = {
		.class = WC_LISTVIEW,
		.style = WS_VISIBLE | LVS_REPORT | LVS_SHAREIMAGELISTS | LVS_SINGLESEL,
	}, [DLG_RAPID] = {
		.class = WC_BUTTON,
		.name = L"Rapid",
		.style = WS_VISIBLE,
	},
};

static void
resize_columns(void)
{
	RECT rect;
	HWND list = GetDlgItem(g_download_list, DLG_LIST);
	GetClientRect(list, &rect);

	int column_rem = rect.right % LENGTH(COLUMN_TITLES);
	int column_width = rect.right / LENGTH(COLUMN_TITLES);

	for (int i=0, n = LENGTH(COLUMN_TITLES); i < n; ++i)
		ListView_SetColumnWidth(list, i, column_width + !i * column_rem);
}

void
DownloadList_remove(const wchar_t *name)
{
	int item = SendDlgItemMessage(g_download_list, DLG_LIST, LVM_FINDITEM, -1,
		(LPARAM)&(LVFINDINFO){.flags = LVFI_STRING, .psz  = (wchar_t *)name}
	);
	SendDlgItemMessage(g_download_list, DLG_LIST, LVM_DELETEITEM, item, 0);
}

void
DownloadList_update(const wchar_t *name, const wchar_t *text)
{
	HWND list = GetDlgItem(g_download_list, DLG_LIST);
	int item = ListView_FindItem(list, -1,
		(&(LVFINDINFO){.flags = LVFI_STRING, .psz  = (wchar_t *)name})
	);

	if (item == -1)
		item = SendMessage(list, LVM_INSERTITEM, 0,
				(LPARAM)&(LVITEM){.mask = LVIF_TEXT, .pszText = (wchar_t *)name});
	SendMessage(list, LVM_SETITEM, 0, (LPARAM)&(LVITEM){
			.mask = LVIF_TEXT,
			.iItem = item,
			.iSubItem = 1,
			.pszText = (wchar_t *)text}
		);
}

static LRESULT CALLBACK
download_list_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch(msg) {
	case WM_CLOSE:
		return 0;
	case WM_CREATE:
		g_download_list = window;
		CreateDlgItems(window, DIALOG_ITEMS, DLG_LAST + 1);

		HWND list_dlg = GetDlgItem(g_download_list, DLG_LIST);

		for (int i=0, n=sizeof(COLUMN_TITLES) / sizeof(char *); i < n; ++i) {
			ListView_InsertColumn(list_dlg, i, (&(LVCOLUMN){
				.mask = LVCF_TEXT | LVCF_SUBITEM,
				.pszText = (wchar_t *)COLUMN_TITLES[i],
				.iSubItem = i,
			}));
		}
		ListView_SetExtendedListViewStyle(list_dlg, LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
	case WM_SIZE: {
		MoveWindow(GetDlgItem(window, DLG_LIST), 0, 0, LOWORD(l_param), HIWORD(l_param) - MAP_Y(14 + 7 + 4), TRUE);
		MoveWindow(GetDlgItem(window, DLG_RAPID), LOWORD(l_param) - MAP_X(50 + 7), HIWORD(l_param) - MAP_Y(14 + 7), MAP_X(50), MAP_Y(14), TRUE);
		resize_columns();
		return 0;
	}
	case WM_COMMAND:
		switch (w_param) {
		case MAKEWPARAM(DLG_RAPID, BN_CLICKED):
			RapidDialog_create();
			return 0;
		}
		return 0;
	}
	return DefWindowProc(window, msg, w_param, l_param);
}


static void __attribute__((constructor))
_init(void)
{
	WNDCLASSEX window_class = {
		.lpszClassName = WC_DOWNLOADTAB,
		.cbSize        = sizeof(WNDCLASSEX),
		.lpfnWndProc   = download_list_proc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};

	RegisterClassEx(&window_class);
}
