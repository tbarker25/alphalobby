#include <assert.h>
#include <inttypes.h>

#include <windows.h>
#include <Commctrl.h>

#include "client_message.h"
#include "common.h"
#include "layoutmetrics.h"
#include "listview.h"
#include "resource.h"

static HWND channelList;

#define LENGTH(x) (sizeof(x) / sizeof(*x))

static void activate(int itemIndex)
{
	wchar_t name[MAX_NAME_LENGTH_NUL];
	ListView_GetItemText(channelList, itemIndex, 0, name, LENGTH(name));
	JoinChannel(utf16to8(name), 1);
}

static void onInit(HWND window)
{
	RECT rect;
	LVCOLUMN columnInfo;

	channelList = GetDlgItem(window, IDC_USERLIST_LIST);
	GetClientRect(channelList, &rect);
	columnInfo .mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH;

	columnInfo.pszText = L"name";
	columnInfo.cx = MAP_X(50);
	columnInfo.iSubItem = 0;
	ListView_InsertColumn(channelList, 0, (LPARAM)&columnInfo);

	columnInfo.pszText = L"users";
	columnInfo.cx = MAP_X(20);
	columnInfo.iSubItem = 1;
	ListView_InsertColumn(channelList, 1, (LPARAM)&columnInfo);

	columnInfo.pszText = L"description";
	columnInfo.cx = rect.right - MAP_X(70) - scrollWidth;
	columnInfo.iSubItem = 2;
	ListView_InsertColumn(channelList, 2, (LPARAM)&columnInfo);

	ListView_SetExtendedListViewStyleEx(channelList,
			LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
}

static BOOL CALLBACK channelListProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		onInit(window);
		return 0;
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(IDOK, BN_CLICKED):
			activate(ListView_GetNextItem(channelList, -1, LVNI_SELECTED));
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
		channelList = NULL;
		return 0;
	default:
		return 0;
	}
}

void ChannelList_AddChannel(const char *name, const char *userCount, const char *desc)
{
	int itemIndex;
	LVITEM item;

	item.mask = LVIF_TEXT;
	item.iSubItem = 0;
	item.pszText = utf8to16(name);
	itemIndex = ListView_InsertItem(channelList, &item);

	assert(itemIndex != -1);

	ListView_SetItemText(channelList, itemIndex, 1, utf8to16(userCount));
	ListView_SetItemText(channelList, itemIndex, 2, utf8to16(desc));
}

void ChannelList_Show(void)
{
	/* DestroyWindow(GetParent(channelList)); */
	assert(channelList == NULL);
	CreateDialog(NULL, MAKEINTRESOURCE(IDD_USERLIST), NULL, channelListProc);
	RequestChannels();
}
