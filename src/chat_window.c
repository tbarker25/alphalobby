#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>
#include <sys\stat.h>

#include <windows.h>
#include <Commctrl.h>

#include "alphalobby.h"
#include "chat_window.h"
#include "data.h"
#include "resource.h"
#include "wincommon.h"

HWND gChatWindow;

static HWND activeTab;
static HWND tabControl;

enum DLG_ID {
	DLG_TAB,
	// DLG_OPENCHANNEL,
	DLG_LAST = DLG_TAB,
};

static const DialogItem dialogItems[] = {
	[DLG_TAB] = {
		.class = WC_TABCONTROL,
		.style = WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
	// }, [DLG_OPENCHANNEL] = {
		// .class = WC_BUTTON,
		// .name = L"Open Channel",
		// .style = WS_CHILD | WS_VISIBLE,
	}
};


static void resizeActiveTab(void)
{
	RECT rect;
	GetClientRect(tabControl, &rect);
	TabCtrl_AdjustRect(tabControl, FALSE, &rect);
	MoveWindow(activeTab, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 1);
}

static int getTabIndex(HWND tabItem)
{
	TCITEM info = {TCIF_PARAM};
	int nbTabs = TabCtrl_GetItemCount(tabControl);
	for (int i=0; i<nbTabs; ++i) {
		TabCtrl_GetItem(tabControl, i, &info);
		if ((HWND)info.lParam == tabItem)
			return i;
	}
	return -1;
}

int ChatWindow_AddTab(HWND tab)
{
	int itemIndex = getTabIndex(tab);
	if  (itemIndex >= 0)
		return itemIndex;
	
	wchar_t windowTitle[MAX_NAME_LENGTH_NUL];
	GetWindowText(tab, windowTitle, MAX_NAME_LENGTH_NUL);
	TCITEM info = {TCIF_TEXT | TCIF_PARAM, .pszText = windowTitle, .lParam = (LPARAM)tab};
	itemIndex = TabCtrl_InsertItem(tabControl, 1000, &info);
	if (itemIndex == 0) {
		activeTab = tab;
		ShowWindow(tab, 1);
		resizeActiveTab();
	}
	// assert(0);
	
	return itemIndex;
}

void ChatWindow_SetActiveTab(HWND tab)
{
	int itemIndex = ChatWindow_AddTab(tab);
	TabCtrl_SetCurSel(tabControl, itemIndex);
	ShowWindow(activeTab, 0);
	activeTab = tab;
	ShowWindow(activeTab, 1);
	resizeActiveTab();
	MainWindow_SetActiveTab(gChatWindow);
}

static LRESULT CALLBACK chatWindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
		gChatWindow = window;
		
		CreateDlgItems(window, dialogItems, DLG_LAST + 1);
		tabControl = GetDlgItem(window, DLG_TAB);
		return 0;
	case WM_SIZE:
		MoveWindow(tabControl, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		// MoveWindow(GetDlgItem, 0, 0, LOWORD(lParam), HIWORD(lParam) - MAP_Y(20), TRUE);
		resizeActiveTab();
		break;
	case WM_CLOSE:
		ShowWindow(window, 0);
		return 0;
	case WM_NOTIFY: {
		NMHDR *info = (void *)lParam;
		if (info->hwndFrom == tabControl && info->code == TCN_SELCHANGE) {
			int tabIndex = TabCtrl_GetCurSel(info->hwndFrom);
			TCITEM info = {TCIF_PARAM};
			TabCtrl_GetItem(tabControl, tabIndex, &info);
			ChatWindow_SetActiveTab((HWND)info.lParam);
			return 0;
		}
	}	break;
	}
	return DefWindowProc(window, msg, wParam, lParam);
}

static void __attribute__((constructor)) init (void)
{
	RegisterClass((&(WNDCLASS){
		.lpszClassName = WC_CHATWINDOW,
		.lpfnWndProc   = chatWindowProc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	}));
}
