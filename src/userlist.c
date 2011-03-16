
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "wincommon.h"

#include <Commctrl.h>

#include "userlist.h"
#include "layoutmetrics.h"
#include "client_message.h"
#include "data.h"
#include "listview.h"
#include "imagelist.h"
#include "sync.h"
#include "common.h"
#include "chat.h"

#include "resource.h"
HWND /* userList,  */chanList;

static BOOL CALLBACK listDialogProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int item;
	switch (msg) {
	case WM_INITDIALOG: {
		HWND list = GetDlgItem(window, IDC_USERLIST_LIST);
		RECT rect;
		GetClientRect(list, &rect);
		const wchar_t *columnNames[] = {L"name", L"users", L"description"};
		for (int i=0; i < lengthof(columnNames); ++i) {
			SendMessage(list, LVM_INSERTCOLUMN, i, (LPARAM)&(LVCOLUMN){
				.mask = LVCF_TEXT | LVCF_SUBITEM,
				// .cx = columnWidth + (i == 0) * (rect.right - scrollWidth) % 3,
				.pszText = (wchar_t *)columnNames[i],
				.iSubItem = i,
			});
		}
		ListView_SetColumnWidth(list, 0, MAP_X(55));
		ListView_SetColumnWidth(list, 1, LVSCW_AUTOSIZE_USEHEADER);
		ListView_SetColumnWidth(list, 2, LVSCW_AUTOSIZE_USEHEADER);
		
		ListView_SetExtendedListViewStyleEx(list, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
	} break;
	case WM_COMMAND: {
		switch (wParam) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
			item = SendDlgItemMessage(window, IDC_USERLIST_LIST, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
			goto activate;
		case MAKEWPARAM(IDCANCEL, BN_CLICKED):
			DestroyWindow(window);
			break;
		}
	} break;
	case WM_NOTIFY: {
		if (((LPNMHDR)lParam)->code == LVN_ITEMACTIVATE) {
			item = ((LPNMITEMACTIVATE)lParam)->iItem;
			goto activate;
		}
	} break;
	case WM_DESTROY: {
		chanList = NULL;
	} break;
	}
	return 0;
	
	activate:;
	wchar_t name[MAX_NAME_LENGTH_NUL];
	SendDlgItemMessage(window, IDC_USERLIST_LIST, LVM_GETITEMTEXT, item,
		(LPARAM)&(LVITEM){.pszText = name, .cchTextMax = lengthof(name)});
	JoinChannel(utf16to8(name), 1);
	DestroyWindow(window);
	return 0;
}

void ChannelList_AddChannel(const char *channame, const char *usercount, const char *description)
{
	int i = SendDlgItemMessage(chanList, IDC_USERLIST_LIST, LVM_INSERTITEM, 0, (LPARAM)&(LVITEM){
			.mask = LVIF_TEXT, .pszText = utf8to16(channame),
		}
	);
	SendDlgItemMessage(chanList, IDC_USERLIST_LIST, LVM_SETITEMTEXT, i, (LPARAM)&(LVITEM){
			.iSubItem = 1, .pszText = utf8to16(usercount),
		}
	);
	SendDlgItemMessage(chanList, IDC_USERLIST_LIST, LVM_SETITEMTEXT, i, (LPARAM)&(LVITEM){
			.iSubItem = 2, .pszText = utf8to16(description),
		}
	);
}

void ChannelList_Show(void)
{
	DestroyWindow(chanList);
	chanList = CreateDialog(NULL, MAKEINTRESOURCE(IDD_USERLIST), NULL, listDialogProc);
	RequestChannels();
}
