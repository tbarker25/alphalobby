
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

//Init in syncThread, then only use in mainthread:
wchar_t gWritableDataDirectory[MAX_PATH];
size_t gWritableDataDirectoryLen;

static HWND currentTab;

enum {
	DLG_TOOLBAR,
	DLG_BATTLELIST,
	DLG_BATTLEROOM,
	DLG_LAST = DLG_BATTLEROOM,
};

enum {
	ID_CONNECT = 0x400,
	ID_BATTLEROOM,
	ID_BATTLELIST,
	ID_SINGLEPLAYER,
	ID_REPLAY,
	ID_HOSTBATTLE,
	ID_OPTIONS
};
	

static const DialogItem dlgItems[] = {
	[DLG_TOOLBAR] {
		.class = TOOLBARCLASSNAME,
		.style = WS_VISIBLE | WS_CHILD | TBSTYLE_FLAT,
	}, [DLG_BATTLELIST] = {
		.name = L"Battle List",
		.class = WC_BATTLELIST,
		.style = WS_VISIBLE,
	}, [DLG_BATTLEROOM] = {
		.class = WC_BATTLEROOM,
		.name = L"Battle Room",
	}
};

static void resizeCurrentTab(int16_t width, int16_t height)
{
	RECT toolbarRect;
	GetClientRect(GetDlgItem(gMainWindow, DLG_TOOLBAR), &toolbarRect);
	
	SetWindowPos(currentTab, NULL, 0, toolbarRect.bottom, width, height - toolbarRect.bottom, 0);
}

void SetCurrentTab(HWND newTab)
{
	if (newTab != currentTab) {
		ShowWindow(currentTab, 0);
		ShowWindow(newTab, 1);
		
		HWND toolbar = GetDlgItem(gMainWindow, DLG_TOOLBAR);
		for (int i=0; i<2; ++i)
			for (int j=0; j<2; ++j)
				if ((i ? newTab : currentTab) == (j ? gBattleListWindow : gBattleRoomWindow))
					SendMessage(toolbar, TB_SETSTATE, j ? ID_BATTLELIST : ID_BATTLEROOM, i ? TBSTATE_ENABLED | TBSTATE_CHECKED : TBSTATE_ENABLED);;
		
		currentTab = newTab;
		
		RECT rect;
		GetClientRect(gMainWindow, &rect);
		resizeCurrentTab(rect.right, rect.bottom);
	}
}

void Ring(void)
{
	FlashWindowEx(&(FLASHWINFO){
		.cbSize = sizeof(FLASHWINFO),
		.hwnd = gMainWindow,
		.dwFlags = 0x00000003 | /* FLASHW_ALL */ 
		           0x0000000C, /* FLASHW_TIMERNOFG */
	});
	SetCurrentTab(gBattleRoomWindow);
}

static LRESULT CALLBACK winMainProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CREATE: {
		gMainWindow = window;
		Chat_Init();
		CreateDlgItems(window, dlgItems, DLG_LAST+1);
		
		HWND toolbar = GetDlgItem(window, DLG_TOOLBAR);
		SendMessage(toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
		SendMessage(toolbar, TB_SETIMAGELIST, 0, (LPARAM)iconList);

		TBBUTTON tbButtons[] = {
			{ ICONS_UNCONNECTED, ID_CONNECT,      TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_DROPDOWN, {}, 0, (INT_PTR)L"Disconnected" },
			{ I_IMAGENONE,  0,               TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_SEP,      {}, 0, 0},
			{ I_IMAGENONE,  ID_BATTLELIST,   TBSTATE_ENABLED, BTNS_AUTOSIZE,                 {}, 0, (INT_PTR)L"Battle List"},
			{ ICONS_BATTLEROOM,  ID_BATTLEROOM,   TBSTATE_ENABLED, BTNS_AUTOSIZE,                 {}, 0, (INT_PTR)L"Battle Room"},
			{ ICONS_SINGLEPLAYER,  ID_SINGLEPLAYER, 0,               BTNS_AUTOSIZE,                 {}, 0, (INT_PTR)L"Single player"},
			{ I_IMAGENONE,  ID_REPLAY,       TBSTATE_ENABLED, BTNS_AUTOSIZE,                 {}, 0, (INT_PTR)L"Replays"},
			{ ICONS_HOSTBATTLE,  ID_HOSTBATTLE,   0,               BTNS_AUTOSIZE,                 {}, 0, (INT_PTR)L"Host Battle"},
			{ I_IMAGENONE,  ID_OPTIONS,      0,          BTNS_AUTOSIZE | BTNS_WHOLEDROPDOWN, {}, 0, (INT_PTR)L"Options"},
			// { I_IMAGENONE, 0, TBSTATE_ENABLED, BTNS_AUTOSIZE|BTNS_WHOLEDROPDOWN, {}, 0, 0},
		};

		SendMessage(toolbar, TB_BUTTONSTRUCTSIZE, sizeof(*tbButtons), 0);
		SendMessage(toolbar, TB_ADDBUTTONS,       sizeof(tbButtons) / sizeof(*tbButtons), (LPARAM)&tbButtons);
		SendMessage(toolbar, TB_AUTOSIZE, 0, 0); 
		
		SetCurrentTab(GetDlgItem(window, DLG_BATTLELIST));
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
		
		MoveWindow(GetDlgItem(window, DLG_TOOLBAR), 0, 0, LOWORD(lParam), 0, 0);
		resizeCurrentTab(LOWORD(lParam), HIWORD(lParam));

		if (dontMoveMinimap)
			return 0;
	}	//FALLTHROUGH:
	case WM_EXITSIZEMOVE:
		dontMoveMinimap = 0;
		SendMessage(gBattleRoomWindow, WM_EXITSIZEMOVE, 0, 0);
		return 0;
	case WM_ENTERSIZEMOVE:
		dontMoveMinimap = 1;
		return 0;
	// case WM_NOTIFY: {
		// const LPNMHDR note = (void *)lParam;
		// if (note->idFrom == DLG_TAB && note->code == TCN_SELCHANGE)
			// setTab(TabCtrl_GetCurSel(note->hwndFrom));
	// }	break;
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(DLG_PROGRESS_BUTTON, BN_CLICKED):
			EndDownload((void *)GetWindowLongPtr((HWND)lParam, GWLP_USERDATA));
			return 0;
		case (ID_CONNECT):
			CreateLoginBox();
			return 0;
		case (ID_BATTLEROOM):
			SetCurrentTab(gBattleRoomWindow);
			return 0;
		case (ID_BATTLELIST):
			SetCurrentTab(gBattleListWindow);
			return 0;
		case (ID_SINGLEPLAYER):
			JoinBattle(-1, NULL);
			return 0;
		case (ID_REPLAY):
			return 0;
		case (ID_HOSTBATTLE):
			CreateHostBattleDlg();
			return 0;
		case (ID_OPTIONS):
			return 0;
		// case IDM_QUIT:
			// DestroyWindow(window);
			// return 0;
		// case IDM_SERVER_LOG:
			// FocusTab(GetServerChat());
			// return 0;
		// case IDM_ABOUT:
			// CreateAboutDlg();
			// return 0;
		// case IDM_OPEN_CHANNEL:
			// ChannelList_Show();
			// return 0;
		// case IDM_SPRING_RTS:
			// ShellExecute(NULL, NULL, L"http://springrts.com/", NULL, NULL, SW_SHOWNORMAL);
			// return 0;
		// case IDM_SPRINGLOBBY:
			// ShellExecute(NULL, NULL, L"http://springlobby.info/landing/index.php", NULL, NULL, SW_SHOWNORMAL);
			// return 0;
		// case IDM_TASCLIENT:
			// ShellExecute(NULL, NULL, L"http://tasclient.licho.eu/TASClientLatest.7z", NULL, NULL, SW_SHOWNORMAL);
			// return 0;
		// case IDM_ZERO_K:
			// ShellExecute(NULL, NULL, L"http://zero-k.info/", NULL, NULL, SW_SHOWNORMAL);
			// return 0;
		// case IDM_UPDATE:
			// ShellExecute(NULL, NULL, L"http://springfiles.com/spring/lobby-clients/alphalobby", NULL, NULL, SW_SHOWNORMAL);
			// return 0;
		// case IDM_RENAME: {
			// char name[MAX_NAME_LENGTH_NUL];
			// *name = '\0';
			// if (!GetTextDlg("Change username", name, MAX_NAME_LENGTH_NUL))
				// RenameAccount(name);
			// } return 0;
		// case IDM_CHANGE_PASSWORD:
			// CreateChangePasswordDlg();
			// return 0;
		// case IDM_REPLAY:
			// CreateReplayDlg();
			// return 0;
		// case IDM_CONNECT:
			// CreateLoginBox();
			// return 0;
		// case IDM_RELOAD_MAPS_MODS:
			// ReloadMapsAndMod();
			// return 0;
		// case IDM_DISCONNECT:
			// Disconnect();
			// return 0;
		// case IDM_LOBBY_PREFERENCES:
			// CreatePreferencesDlg();
			// return 0;
		// case IDM_SPRING_SETTINGS:
			// CreateProcess(L"springsettings.exe", L"springsettings.exe", NULL, NULL, 0, 0, NULL,NULL,
				// &(STARTUPINFO){.cb=sizeof(STARTUPINFO)},
				// &(PROCESS_INFORMATION){}
			// );
			// return 0;
		// case IDM_HOST_BATTLE:
			// CreateHostBattleDlg();
			// return 0;
		// case IDM_SINGLE_PLAYER:
			// JoinBattle(-1, NULL);
			// return 0;
		}
		return 0;
	case WM_MAKE_MESSAGEBOX:
		MessageBoxA(window, (char *)wParam, (char *)lParam, 0);
		free((void *)wParam);
		free((void *)lParam);
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

void MyMessageBox(const char *caption, const char *text)
{
	PostMessage(gMainWindow, WM_MAKE_MESSAGEBOX, (WPARAM)strdup(text), (WPARAM)strdup(caption));
}

static const int connectedOnlyMenuItems[] = {
	IDM_RENAME, IDM_CHANGE_PASSWORD, IDM_HOST_BATTLE, IDM_OPEN_CHANNEL
};

void MainWindow_ChangeConnect(int isNowConnected)
{
	HMENU menu = GetMenu(gMainWindow);
	SendDlgItemMessage(gMainWindow, DLG_TOOLBAR, TB_SETBUTTONINFO, IDM_CONNECT,
		(LPARAM)&(TBBUTTONINFO){
			.cbSize = sizeof(TBBUTTONINFO),
			.dwMask = TBIF_IMAGE | TBIF_STATE | TBIF_TEXT,
			.iImage = isNowConnected ? ICONS_UNCONNECTED : ICONS_UNCONNECTED,
			.fsState = isNowConnected ? TBSTATE_ENABLED : TBSTATE_ENABLED,
			.pszText = isNowConnected ? L"Connected" : L"Unconnected",
		});

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
	});
	LONG left = CW_USEDEFAULT, top = CW_USEDEFAULT, width = CW_USEDEFAULT, height = CW_USEDEFAULT;
	const char *windowPlacement = LoadSetting("window_placement");
	if (windowPlacement)
		sscanf(windowPlacement, "%ld,%ld,%ld,%ld", &left, &top, &width, &height);

	CreateWindowEx(0, WC_ALPHALOBBY, L"AlphaLobby", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		left, top, width, height,
		NULL, (HMENU)0, NULL, NULL);


	char username[MAX_NAME_LENGTH_NUL], *s;
	if (gSettings.flags & SETTING_AUTOCONNECT
			&& (s = LoadSetting("username")) && strcpy(username, s)
			&& (s = LoadSetting("password")))
		Login(username, s);

	Sync_Init();
	
	
	
    for (MSG msg; GetMessage(&msg, NULL, 0, 0) > 0; ) {
		if (msg.message == WM_KEYDOWN && msg.wParam == VK_F1)
			CreateAboutDlg();
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
