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
#include <Commctrl.h>

#include "dialogboxes.h"
#include "downloadtab.h"
#include "layoutmetrics.h"
#include "resource.h"
#include "wincommon.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

enum DLG_ID {
	DLG_LIST,
	DLG_RAPID, 
	DLG_LAST = DLG_RAPID,
};

HWND gDownloadTabWindow;

static const wchar_t *const columns[] = {L"Name", L"Status"};

static const DialogItem dialogItems[] = {
	[DLG_LIST] = {
		.class = WC_LISTVIEW,
		.style = WS_VISIBLE | LVS_REPORT | LVS_SHAREIMAGELISTS | LVS_SINGLESEL,
	}, [DLG_RAPID] = {
		.class = WC_BUTTON,
		.name = L"Rapid",
		.style = WS_VISIBLE,
	},
};

static void resizeColumns(void)
{
	RECT rect;
	HWND list = GetDlgItem(gDownloadTabWindow, DLG_LIST);
	GetClientRect(list, &rect);

	int columnRem = rect.right % LENGTH(columns);
	int columnWidth = rect.right / LENGTH(columns);

	for (int i=0, n = LENGTH(columns); i < n; ++i)
		ListView_SetColumnWidth(list, i, columnWidth + !i * columnRem);
}

void RemoveDownload(const wchar_t *name)
{
	int item = SendDlgItemMessage(gDownloadTabWindow, DLG_LIST, LVM_FINDITEM, -1,
		(LPARAM)&(LVFINDINFO){.flags = LVFI_STRING, .psz  = (wchar_t *)name}
	);
	SendDlgItemMessage(gDownloadTabWindow, DLG_LIST, LVM_DELETEITEM, item, 0);
}

void UpdateDownload(const wchar_t *name, const wchar_t *text)
{
	HWND list = GetDlgItem(gDownloadTabWindow, DLG_LIST);
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

static LRESULT CALLBACK downloadTabProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CLOSE:
		return 0;
	case WM_CREATE:
		gDownloadTabWindow = window;
		CreateDlgItems(window, dialogItems, DLG_LAST + 1);
		
		HWND listDlg = GetDlgItem(gDownloadTabWindow, DLG_LIST);
		
		for (int i=0, n=sizeof(columns) / sizeof(char *); i < n; ++i) {
			ListView_InsertColumn(listDlg, i, (&(LVCOLUMN){
				.mask = LVCF_TEXT | LVCF_SUBITEM,
				.pszText = (wchar_t *)columns[i],
				.iSubItem = i,
			}));
		}
		ListView_SetExtendedListViewStyle(listDlg, LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
	case WM_SIZE: {
		MoveWindow(GetDlgItem(window, DLG_LIST), 0, 0, LOWORD(lParam), HIWORD(lParam) - MAP_Y(14 + 7 + 4), TRUE);
		MoveWindow(GetDlgItem(window, DLG_RAPID), LOWORD(lParam) - MAP_X(50 + 7), HIWORD(lParam) - MAP_Y(14 + 7), MAP_X(50), MAP_Y(14), TRUE);
		resizeColumns();
		return 0;
	}
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(DLG_RAPID, BN_CLICKED):
			CreateRapidDlg();
			return 0;
		}
		return 0;
	}
	return DefWindowProc(window, msg, wParam, lParam);
}


static void __attribute__((constructor)) _init_ (void)
{
	RegisterClassEx((&(WNDCLASSEX){
		.lpszClassName = WC_DOWNLOADTAB,
		.cbSize        = sizeof(WNDCLASSEX),
		.lpfnWndProc   = downloadTabProc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	}));
}

