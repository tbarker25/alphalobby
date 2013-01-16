#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>

#include <windows.h>
#include <Commctrl.h>

#include "common.h"
#include "imagelist.h"
#include "layoutmetrics.h"
#include "listview.h"
#include "resource.h"
#include "user.h"
#include "chat_window.h"
#include "chat.h"

static HWND userList;

#define LENGTH(x) (sizeof(x) / sizeof(*x))

static void activate(int itemIndex)
{
	LVITEM itemInfo;
	itemInfo.mask = LVIF_PARAM;
	itemInfo.iItem = itemIndex;
	itemInfo.iSubItem = 2;
	ListView_GetItem(userList, &itemInfo);
	ChatWindow_SetActiveTab(GetPrivateChat((User *)itemInfo.lParam));
}

static void onInit(HWND window)
{
	RECT rect;
	LVCOLUMN columnInfo;

	DestroyWindow(GetParent(userList));
	userList = GetDlgItem(window, IDC_USERLIST_LIST);

	SetWindowLongPtr(userList, GWL_STYLE, WS_VISIBLE | LVS_SHAREIMAGELISTS
			| LVS_SINGLESEL | LVS_REPORT | LVS_SORTASCENDING |
			LVS_NOCOLUMNHEADER);

	ListView_SetExtendedListViewStyle(userList, LVS_EX_DOUBLEBUFFER | LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
	EnableIcons(userList);

	GetClientRect(userList, &rect);
	columnInfo.mask = LVCF_SUBITEM | LVCF_WIDTH;

	columnInfo.cx = rect.right - ICON_SIZE - scrollWidth;
	columnInfo.iSubItem = 0;
	ListView_InsertColumn(userList, 0, (LPARAM)&columnInfo);

	columnInfo.cx = ICON_SIZE;
	columnInfo.iSubItem = 1;
	ListView_InsertColumn(userList, 1, (LPARAM)&columnInfo);

	for (const User *u; (u = GetNextUser());) {
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

		item.iImage = u->clientStatus & CS_INGAME_MASK ? ICONS_INGAME
			: u->battle ? INGAME_MASK
			: -1;

		int imageIndex = USER_MASK;
		if (u->clientStatus & CS_AWAY_MASK)
		imageIndex |= AWAY_MASK;
		if (u->ignore)
		imageIndex |= IGNORE_MASK;
		item.state = INDEXTOOVERLAYMASK(imageIndex);
		item.stateMask = LVIS_OVERLAYMASK;

		item.iItem = ListView_InsertItem(userList, &item);

		item.mask = LVIF_IMAGE;
		item.iImage = ICONS_FIRST_FLAG + u->country;
		item.iSubItem = 1;
		ListView_SetItem(userList, &item);
	}
}

static BOOL CALLBACK userListProc(HWND window, UINT msg, WPARAM wParam,
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

void UserList_Show(void)
{
	CreateDialog(NULL, MAKEINTRESOURCE(IDD_USERLIST), NULL, userListProc);
}
