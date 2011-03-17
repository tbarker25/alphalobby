
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "wincommon.h"
#include "resource.h"

#include <Commctrl.h>

#include "alphalobby.h"
#include "client.h"
#include "client_message.h"
#include "battlelist.h"
#include "battleroom.h"
#include "userlist.h"
#include "dialogboxes.h"
#include "chat.h"
#include "downloader.h"
#include "layoutmetrics.h"
#include "settings.h"
#include "sync.h"
#include "md5.h"
#include "imagelist.h"

#define WC_ALPHALOBBY L"AlphaLobby"

HWND gMainWindow;
HWND tabControl, statusBar, tabItem;
RECT tabArea;
//Init in syncThread, then only use in mainthread:
wchar_t gWritableDataDirectory[MAX_PATH];
size_t gWritableDataDirectoryLen;

enum {
	DLG_TAB,
	DLG_STATUSBAR,
	DLG_BATTLELIST,
	DLG_BATTLEROOM,
	DLG_LAST = DLG_BATTLEROOM,
};

static const DialogItem dlgItems[] = {
	[DLG_TAB] = {
		.class = WC_TABCONTROL,
		.style = WS_VISIBLE |WS_CLIPCHILDREN,
	}, [DLG_STATUSBAR] = {
		.class = STATUSCLASSNAME,
		.style = WS_VISIBLE | WS_CLIPSIBLINGS,
	}, [DLG_BATTLELIST] = {
		.name = L"Battle List",
		.class = WC_BATTLELIST,
		.style = WS_VISIBLE,
	}, [DLG_BATTLEROOM] = {
		.class = WC_BATTLEROOM,
		.name = L"Battle Room",
	}
};

static void resizeTabControl(void)
{
	GetClientRect(tabControl, &tabArea);
	TabCtrl_AdjustRect(tabControl, 0, &tabArea);
	MapWindowPoints(tabControl, gMainWindow, (POINT *)&tabArea, 2);
	
	TCITEM item = {.mask = TCIF_PARAM};
	for (int i=0,total=TabCtrl_GetItemCount(tabControl); i<total; ++i) {
		TabCtrl_GetItem(tabControl, i, &item);
		RECT rect;
		TabCtrl_GetItemRect(tabControl, i, &rect);
		SetWindowPos(GetDlgItem(tabControl, i), 0, rect.right - rect.bottom - rect.top + 4, rect.top+2, rect.bottom - rect.top-4, rect.bottom - rect.top-4, (HWND)item.lParam == gBattleListHandle ? SWP_HIDEWINDOW : SWP_SHOWWINDOW);
	}
}

static void resizeTabItem(void)
{
	SetWindowPos(tabItem, NULL, tabArea.left, tabArea.top, tabArea.right - tabArea.left, tabArea.bottom - tabArea.top, 0);
}

static void setTab(int i)
{
	
	TCITEM item = {.mask = TCIF_PARAM};
	if (!TabCtrl_GetItem(tabControl, i, &item)
			|| tabItem == (HWND)item.lParam)
		return;
	
	ShowWindow(tabItem, 0);
	tabItem = (HWND)item.lParam;
	
	ShowWindow(tabItem, SW_SHOW);
	TabCtrl_SetCurSel(tabControl, i);

	resizeTabItem();
}

int GetTabIndex(HWND window)
{
	TCITEM item = {.mask = TCIF_PARAM};
	for (int index=TabCtrl_GetItemCount(tabControl); index >= 0; --index) {
		TabCtrl_GetItem(tabControl, index, &item);
		if ((HWND)item.lParam == window)
			return index;
	}
	return -1;
}

void AttachTab(HWND window)
{
	SetParent(window, gMainWindow);
	SetWindowLongPtr(window, GWL_STYLE, WS_CHILD);
	AddTab(window, 1);
}

void DetatchTab(HWND window)
{
	RemoveTab(window);
	SetWindowLongPtr(window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
	SetParent(window, NULL);
	return;
}

void AddTab(HWND window, int focus)
{
	if (!GetParent(window)) {
		if (focus)
			BringWindowToTop(window);
		return;
	}

	TCITEM item = {.mask = TCIF_PARAM};
	int index = GetTabIndex(window);
	if (index >= 0)
		goto end;
	index = TabCtrl_GetItemCount(tabControl);
	extern HWND gBattleRoomWindow;
	wchar_t text[128];
	wcscpy(text + GetWindowText(window, text, sizeof(text)), index ? L"     " : L"");

	item = (TCITEM){
			.mask = TCIF_TEXT | TCIF_PARAM,
			.pszText = text,
			.lParam = (LPARAM)window,
		};
	HWND button = CreateWindowEx(WS_EX_TOPMOST, WC_BUTTON, NULL, WS_VISIBLE | WS_CHILD | BS_ICON, 0,0,0,0, tabControl, (HMENU)index, NULL, 0);
	SendMessage(button, BM_SETIMAGE, IMAGE_ICON, (WPARAM)ImageList_GetIcon(iconList, ICONS_CLOSEBUTTON, 0));
	index = TabCtrl_InsertItem(tabControl, window == gBattleRoomWindow ?: index, &item);
	end:
	if (focus)
		setTab(index);
	resizeTabControl();
}



void RemoveTab(HWND window)
{
	TCITEM item = {.mask = TCIF_PARAM};
	for (int i=0,total=TabCtrl_GetItemCount(tabControl); i<total; ++i) {
		TabCtrl_GetItem(tabControl, i, &item);
		if ((HWND)item.lParam == window) {
			int toSet = TabCtrl_GetCurSel(tabControl) == i;
			DestroyWindow(GetDlgItem(tabControl, total-1));
			TabCtrl_DeleteItem(tabControl, i);
			
			if (toSet)
				setTab(i - !!i);
		}
	}
	SetParent(window, gMainWindow);
	SetWindowLongPtr(window, GWL_STYLE, WS_CHILD);
	resizeTabControl();
}

wchar_t status[128];

void UpdateStatusBar(void)
{
	#define STATUS_WIDTH MAP_X(100)
	RECT rect;
	
	GetClientRect(statusBar, &rect);
	
	int parts[nbDownloads+1];
	for (int i=0; i<lengthof(parts)-1; ++i)
		parts[i] = (rect.right - STATUS_WIDTH - rect.bottom) * (i + 1) / (lengthof(parts)-1);
	parts[lengthof(parts)-1] = rect.right - rect.bottom;
	
	SendMessage(statusBar, SB_SETPARTS, lengthof(parts), (LPARAM)parts);
	SendMessage(statusBar, SB_SETTEXT, lengthof(parts)-1, (LPARAM)status);
	int n=0;
	void func(HWND progressBar, HWND button, const wchar_t *text)
	{
		HDC dc = GetDC(statusBar);
		SelectObject(dc, gFont);
		SIZE size;
		GetTextExtentPoint32(dc, text, wcslen(text), &size);
		ReleaseDC(statusBar, dc);
		RECT itemRect;
		SendMessage(statusBar, SB_SETTEXT, n, (LPARAM)text);
		SendMessage(statusBar, SB_GETRECT, n, (LPARAM)&itemRect);
		MapWindowPoints(statusBar, gMainWindow, (POINT *)&itemRect, 2);
		SetWindowPos(progressBar, HWND_TOP,
				itemRect.left + size.cx + RELATED_SPACING_X,
				(itemRect.bottom + itemRect.top - PROGRESS_Y) / 2,
				itemRect.right - itemRect.left - size.cx - COMMANDBUTTON_X - 3 * RELATED_SPACING_X,
				PROGRESS_Y, 0);
		SetWindowPos(button, HWND_TOP,
				itemRect.right - COMMANDBUTTON_X - 2 * RELATED_SPACING_X,
				(itemRect.bottom + itemRect.top - COMMANDBUTTON_Y) / 2,
				COMMANDBUTTON_X,
				COMMANDBUTTON_Y, 0);
		++n;
	
	}
	ForEachDownload(func);
}

void SetStatus(const wchar_t *text)
{
	wcscpy(status, text);
	UpdateStatusBar();
}

void Ring(void)
{
	FlashWindowEx(&(FLASHWINFO){
		.cbSize = sizeof(FLASHWINFO),
		.hwnd = gMainWindow,
		.dwFlags = 0x00000003 | /* FLASHW_ALL */ 
		           0x0000000C, /* FLASHW_TIMERNOFG */
	});
	AddTab(gBattleRoomWindow, 1);
}

static LRESULT CALLBACK tabControlProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR unused)
{
	static int currentTab;	//Can be static since only one window can be dragged at once.
	switch (msg) {
	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED) {
			TCITEM item = {TCIF_PARAM};
			TabCtrl_GetItem(tabControl, LOWORD(wParam), &item);
			SendMessage((HWND)item.lParam, WM_CLOSE, 0, 0);
			return 0;
		}
		break;
	case WM_LBUTTONDOWN:
		currentTab = TabCtrl_HitTest(window, (&(TCHITTESTINFO){{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}}));
		SetCapture(window);
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		break;
	case WM_MOUSEMOVE: {
		int newTab = SendMessage(window, TCM_HITTEST, 0, (LPARAM)&(TCHITTESTINFO){{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}});
		if (newTab < 0 || newTab == currentTab || GetCapture() != window)
			break;
			
		//If we swap diffent sized tabs they might end up alternating rapidly, make sure this won't happen:
		RECT rect;
		TabCtrl_GetItemRect(tabControl, newTab, &rect);
		lParam -= rect.left;
		TabCtrl_GetItemRect(tabControl, currentTab, &rect);
		if (rect.right - rect.left <= GET_X_LPARAM(lParam)) //True -> tabs would alternate
			return 0; //dont call DefSubclassProc to avoid highlighting newTab

		wchar_t buff[128];
		TCITEM item = {TCIF_PARAM | TCIF_TEXT, .pszText = buff, lengthof(buff)};
		TabCtrl_GetItem(tabControl, currentTab, &item);
		TabCtrl_DeleteItem(tabControl, currentTab);
		currentTab = TabCtrl_InsertItem(tabControl, newTab, &item);
		resizeTabControl();
	} break;
	}
	
	return DefSubclassProc(window, msg, wParam, lParam);
}

static LRESULT CALLBACK winMainProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CREATE: {
		gMainWindow = window;
		CreateDlgItems(window, dlgItems, DLG_LAST+1);
		tabControl = GetDlgItem(window, DLG_TAB);
		statusBar = GetDlgItem(gMainWindow, DLG_STATUSBAR);
		AddTab(GetDlgItem(window, DLG_BATTLELIST), 1);
		SetStatus(L"\t\tNot Connected");
		SetWindowSubclass(tabControl, tabControlProc, 0, 0);
		UpdateWindow(tabControl);
	}	break;
	case WM_DESTROY: {
		Disconnect();
		SaveLastChatWindows();
		WINDOWPLACEMENT windowPlacement;
		windowPlacement.length = sizeof(windowPlacement);
		GetWindowPlacement(gMainWindow, &windowPlacement);
		RECT *r = &windowPlacement.rcNormalPosition;
		r->bottom -= r->top;
		r->right -= r->left;
		if (windowPlacement.showCmd == SW_MAXIMIZE) {
			r->left = CW_USEDEFAULT;
			r->top = SW_SHOWMAXIMIZED;
		}
		char windowPlacementText[128];
		sprintf(windowPlacementText, "%ld,%ld,%ld,%ld", r->left, r->top, r->right, r->bottom);
		SaveSetting("window_placement", windowPlacementText);
		UnitSync_Cleanup();
		SaveAliases();
		PostQuitMessage(0);
	}	return 0;
	static int dontMoveMinimap;
	case WM_SIZE: {
		RECT statusBarRect;
		SendMessage(statusBar, WM_SIZE, 0, 0);
		UpdateStatusBar();
		GetClientRect(statusBar, &statusBarRect);

		SetWindowPos(tabControl, HWND_BOTTOM, 0, 0, LOWORD(lParam), HIWORD(lParam) - statusBarRect.bottom, SWP_NOREDRAW);
		resizeTabControl();
		resizeTabItem();
		if (dontMoveMinimap)
			return 0;
	}	//FALLTHROUGH:
	case WM_EXITSIZEMOVE:
		dontMoveMinimap = 0;
		InvalidateRect(tabControl, 0, 0);
		SendMessage(gBattleRoomWindow, WM_EXITSIZEMOVE, 0, 0);
		return 0;
	case WM_ENTERSIZEMOVE:
		dontMoveMinimap = 1;
		return 0;
	case WM_NOTIFY: {
		const LPNMHDR note = (void *)lParam;
		if (note->idFrom == DLG_TAB && note->code == TCN_SELCHANGE)
			setTab(TabCtrl_GetCurSel(note->hwndFrom));
	}	break;
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(DLG_PROGRESS_BUTTON, BN_CLICKED):
			EndDownload((void *)GetWindowLongPtr((HWND)lParam, GWLP_USERDATA));
			return 0;
		case IDM_QUIT:
			DestroyWindow(window);
			return 0;
		case IDM_SERVER_LOG:
			AddTab(GetServerChat(), 1);
			return 0;
		case IDM_ABOUT:
			CreateAboutDlg();
			return 0;
		case IDM_OPEN_CHANNEL:
			ChannelList_Show();
			return 0;
		case IDM_SPRING_RTS:
			ShellExecute(NULL, NULL, L"http://springrts.com/", NULL, NULL, SW_SHOWNORMAL);
			return 0;
		case IDM_SPRINGLOBBY:
			ShellExecute(NULL, NULL, L"http://springlobby.info/landing/index.php", NULL, NULL, SW_SHOWNORMAL);
			return 0;
		case IDM_TASCLIENT:
			ShellExecute(NULL, NULL, L"http://tasclient.licho.eu/TASClientLatest.7z", NULL, NULL, SW_SHOWNORMAL);
			return 0;
		case IDM_ZERO_K:
			ShellExecute(NULL, NULL, L"http://zero-k.info/", NULL, NULL, SW_SHOWNORMAL);
			return 0;
		case IDM_UPDATE:
			ShellExecute(NULL, NULL, L"http://springfiles.com/spring/lobby-clients/alphalobby", NULL, NULL, SW_SHOWNORMAL);
			return 0;
		case IDM_RENAME: {
			char name[MAX_NAME_LENGTH_NUL];
			*name = '\0';
			if (!GetTextDlg("Change username", name, MAX_NAME_LENGTH_NUL))
				RenameAccount(name);
			} return 0;
		case IDM_CHANGE_PASSWORD:
			CreateChangePasswordDlg();
			return 0;
		case IDM_REPLAY:
			CreateReplayDlg();
			return 0;
		case IDM_CONNECT:
			CreateLoginBox();
			return 0;
		case IDM_RELOAD_MAPS_MODS:
			ReloadMapsAndMod();
			return 0;
		case IDM_DISCONNECT:
			Disconnect();
			return 0;
		case IDM_LOBBY_PREFERENCES:
			CreatePreferencesDlg();
			return 0;
		case IDM_SPRING_SETTINGS:
			CreateProcess(L"springsettings.exe", L"springsettings.exe", NULL, NULL, 0, 0, NULL,NULL,
				&(STARTUPINFO){.cb=sizeof(STARTUPINFO)},
				&(PROCESS_INFORMATION){}
			);
			return 0;
		case IDM_HOST_BATTLE:
			CreateHostBattleDlg();
			return 0;
		case IDM_SINGLE_PLAYER:
			JoinBattle(-1, NULL);
			return 0;
		}
		return 0;
	case WM_MAKE_MESSAGEBOX:
		MessageBoxA(window, (char *)wParam, (char *)lParam, 0);
		return 0;
	case WM_DESTROY_WINDOW:
		DestroyWindow((HWND)lParam);
		return 0;
	case WM_POLL_SERVER:
		PollServer();
		return 0;
	case WM_EXEC_FUNC:
		((void (*)(LPARAM))wParam)(lParam);
		return 0;
	case WM_CREATE_DLG_ITEM:
		return (LRESULT)CreateDlgItem(window, (void *)lParam, wParam);
	case WM_TIMER:
		if (wParam == 1) {
			// Ping();
			return 0;
		}
		break;
	}
	return DefWindowProc(window, msg, wParam, lParam);
}

static const int connectedOnlyMenuItems[] = {
	IDM_RENAME, IDM_CHANGE_PASSWORD, IDM_HOST_BATTLE, IDM_OPEN_CHANNEL
};

void MainWindow_ChangeConnect(int isNowConnected)
{
	HMENU menu = GetMenu(gMainWindow);
	for (int i=0; i < lengthof(connectedOnlyMenuItems); ++i)
		EnableMenuItem(menu, connectedOnlyMenuItems[i], !isNowConnected * MF_GRAYED);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	InitSettings();
	
	srand(time(NULL));

	InitializeSystemMetrics();
	LoadLibrary(L"Riched20.dll");
	
	RegisterClassEx(&(WNDCLASSEX){
		.cbSize = sizeof(WNDCLASSEX),
		.lpszClassName = WC_ALPHALOBBY,
		.lpfnWndProc	= winMainProc,
		.hIcon          = ImageList_GetIcon(iconList, 0, 0),
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
		.lpszMenuName = MAKEINTRESOURCE(IDR_MENU),
	});
	LONG left = CW_USEDEFAULT, top = CW_USEDEFAULT, width = CW_USEDEFAULT, height = CW_USEDEFAULT;
	const char *windowPlacement = LoadSetting("window_placement");
	if (windowPlacement)
		sscanf(windowPlacement, "%ld,%ld,%ld,%ld", &left, &top, &width, &height);

	CreateWindowEx(0, WC_ALPHALOBBY, L"AlphaLobby", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		left, top, width, height,
		NULL, (HMENU)0, NULL, NULL);
	// if (maximize)
		// ShowWindow(gMainWindow, SW_SHOWMAXIMIZED);
		
	InvalidateRect(tabControl, 0, 0);
	char username[MAX_NAME_LENGTH_NUL], *s;
	if (gSettings.flags & SETTING_AUTOCONNECT
			&& (s = LoadSetting("username")) && strcpy(username, s)
			&& (s = LoadSetting("password")))
		Login(username, s);

	Sync_Init();

    for (MSG msg; GetMessage(&msg, NULL, 0, 0) > 0; ) {
		if (msg.message == WM_KEYDOWN && msg.wParam == VK_F1)
			CreateAboutDlg();
		if (msg.message == WM_KEYDOWN && GetKeyState(VK_CONTROL) & 0x80) {
			switch (msg.wParam) {
			case 'Q': {
				HWND window = GetForegroundWindow();
				if (window == gMainWindow)
					DetatchTab(tabItem);
				else
					AttachTab(window);
			}	break;
			case 'W':
				SendMessage(tabItem, WM_CLOSE, 0, 0);
				continue;
			case VK_LEFT:
				setTab(TabCtrl_GetCurSel(tabControl) - 1);
				continue;
			case VK_RIGHT:
				setTab(TabCtrl_GetCurSel(tabControl) + 1);
				continue;
			case '1'...'9':
				setTab(msg.wParam - '1');
				continue;
			}
		}
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
	
    return 0;
}

const wchar_t * GetWritablePath_unsafe(wchar_t *file)
{
	wcscpy(gWritableDataDirectory + gWritableDataDirectoryLen, file ?: L"");
	return gWritableDataDirectory;
}

void GetWritablePath(wchar_t *file, wchar_t *buff)
{
	memcpy(buff, gWritableDataDirectory, gWritableDataDirectoryLen * sizeof(wchar_t));
	wcscpy(buff + gWritableDataDirectoryLen, file ?: L"");
}
