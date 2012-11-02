#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

#include <windows.h>
#include <windowsx.h>
#include <oleacc.h>
#include <Commctrl.h>
#include "wincommon.h"


#include <Commctrl.h>
#include <Wingdi.h>
#include <assert.h>
#include <richedit.h>

#include "alphalobby.h"
#include "battleroom.h"
#include "chat.h"
#include "usermenu.h"
#include "client_message.h"
#include "chat.h"
#include "data.h"
#include "countrycodes.h"
#include "imagelist.h"
#include "dialogboxes.h"
#include "sync.h"
#include "common.h"
#include "settings.h"
#include "downloadtab.h"
#include "layoutmetrics.h"
#include "downloader.h"
#include "battletools.h"
#include "listview.h"

#include "spring.h"

#include "resource.h"

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

