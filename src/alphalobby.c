
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "wincommon.h"
#include "resource.h"

#include <Commctrl.h>
#include <assert.h>

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
#include "downloadtab.h"
#include "chat_window.h"

#define WC_ALPHALOBBY L"AlphaLobby"

#ifndef NDEBUG
#define TBSTATE_DISABLED TBSTATE_ENABLED
#else
#define TBSTATE_DISABLED 0
#endif

HWND gMainWindow;

//Init in syncThread, then only use in mainthread:
wchar_t gWritableDataDirectory[MAX_PATH];
size_t gWritableDataDirectoryLen;

static HWND currentTab;

enum {
	DLG_TOOLBAR,
	DLG_BATTLELIST,
	DLG_BATTLEROOM,
	DLG_CHAT,
	DLG_DOWNLOADTAB,
	DLG_LAST = DLG_DOWNLOADTAB,
};

enum {
	ID_CONNECT = 0x400,
	ID_BATTLEROOM,
	ID_BATTLELIST,
	ID_SINGLEPLAYER,
	ID_REPLAY,
	ID_HOSTBATTLE,
	ID_CHAT,
	ID_OPTIONS,
	ID_DOWNLOADS,
	
	ID_LOGINBOX,
	ID_SERVERLOG,
	ID_LOBBY_PREFERENCES,
	ID_SPRING_SETTINGS,
};
	

static const DialogItem dialogItems[] = {
	[DLG_TOOLBAR] {
		.class = TOOLBARCLASSNAME,
		.style = WS_VISIBLE | WS_CHILD | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT,
	}, [DLG_BATTLELIST] = {
		.class = WC_BATTLELIST,
		.style = WS_VISIBLE,
	}, [DLG_BATTLEROOM] = {
		.class = WC_BATTLEROOM,
	}, [DLG_CHAT] = {
		.name = L"Chat Window",
		.class = WC_CHATWINDOW,
	}, [DLG_DOWNLOADTAB] = {
		.class = WC_DOWNLOADTAB,
	}
};

static void resizeCurrentTab(int16_t width, int16_t height)
{
	RECT toolbarRect;
	GetClientRect(GetDlgItem(gMainWindow, DLG_TOOLBAR), &toolbarRect);
	
	SetWindowPos(currentTab, NULL, 0, toolbarRect.bottom, width, height - toolbarRect.bottom, 0);
}

void MainWindow_SetActiveTab(HWND newTab)
{
	if (newTab == currentTab)
		return;
	
	void changeCheck(bool enable) {
		int tabIndex = currentTab == gBattleListWindow  ? ID_BATTLELIST
	                 : currentTab == gBattleRoomWindow  ? ID_BATTLEROOM
		             : currentTab == gChatWindow        ? ID_CHAT
					 : currentTab == gDownloadTabWindow ? ID_DOWNLOADS
		             : -1;
		WPARAM state = SendDlgItemMessage(gMainWindow, DLG_TOOLBAR, TB_GETSTATE, tabIndex, 0);
		if (enable)
			state |= TBSTATE_CHECKED;
		else
			state &= ~TBSTATE_CHECKED;
		SendDlgItemMessage(gMainWindow, DLG_TOOLBAR, TB_SETSTATE, tabIndex, state);
		ShowWindow(currentTab, enable);
	}
	changeCheck(0);
	currentTab = newTab;
	changeCheck(1);
	
	RECT rect;
	GetClientRect(gMainWindow, &rect);
	resizeCurrentTab(rect.right, rect.bottom);
}

void Ring(void)
{
	FlashWindowEx(&(FLASHWINFO){
		.cbSize = sizeof(FLASHWINFO),
		.hwnd = gMainWindow,
		.dwFlags = 0x00000003 | /* FLASHW_ALL */ 
		           0x0000000C, /* FLASHW_TIMERNOFG */
	});
	MainWindow_SetActiveTab(gBattleRoomWindow);
}

void MainWindow_DisableBattleroomButton(void)
{
	if (currentTab == gBattleRoomWindow)
		MainWindow_SetActiveTab(gBattleListWindow);
	SendDlgItemMessage(gMainWindow, DLG_TOOLBAR, TB_SETSTATE, ID_BATTLEROOM, TBSTATE_DISABLED);
}

void MainWindow_EnableBattleroomButton(void)
{
	SendDlgItemMessage(gMainWindow, DLG_TOOLBAR, TB_SETSTATE, ID_BATTLEROOM, TBSTATE_ENABLED | TBSTATE_CHECKED);
	MainWindow_SetActiveTab(gBattleRoomWindow);
}


static LRESULT CALLBACK winMainProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CREATE: {
		gMainWindow = window;
		CreateDlgItems(window, dialogItems, DLG_LAST+1);
		
		HWND toolbar = GetDlgItem(window, DLG_TOOLBAR);
		SendMessage(toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
		SendMessage(toolbar, TB_SETIMAGELIST, 0, (LPARAM)gIconList);
		

		TBBUTTON tbButtons[] = {
			{ ICONS_OFFLINE,     ID_CONNECT,      TBSTATE_ENABLED, BTNS_DROPDOWN, {}, 0, (INT_PTR)L"Offline" },
			{ I_IMAGENONE,       0,               TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_SEP, {}, 0, 0},
			{ ICONS_BATTLELIST,  ID_BATTLELIST,   TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Battle List"},
			
			{ ICONS_BATTLEROOM,  ID_BATTLEROOM,   TBSTATE_DISABLED,BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Battle Room"},
			#ifndef NDEBUG
			{ ICONS_SINGLEPLAYER,ID_SINGLEPLAYER, TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Single player"},
			{ ICONS_REPLAY,      ID_REPLAY,       TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Replays"},
			{ ICONS_HOSTBATTLE,  ID_HOSTBATTLE,   TBSTATE_DISABLED,BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Host Battle"},
			#endif
			{ I_IMAGENONE,       ID_CHAT,         TBSTATE_DISABLED,BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Chat"},
			// { I_IMAGENONE,       ID_CHAT,      TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_WHOLEDROPDOWN, {}, 0, (INT_PTR)L"Users"},
			{ ICONS_OPTIONS,     ID_OPTIONS,      TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_WHOLEDROPDOWN, {}, 0, (INT_PTR)L"Options"},
			{ I_IMAGENONE,       ID_DOWNLOADS,    TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Downloads"},
		};
		

		SendMessage(toolbar, TB_BUTTONSTRUCTSIZE, sizeof(*tbButtons), 0);
		SendMessage(toolbar, TB_ADDBUTTONS,       sizeof(tbButtons) / sizeof(*tbButtons), (LPARAM)&tbButtons);
		SendMessage(toolbar, TB_AUTOSIZE, 0, 0); 
		
		MainWindow_SetActiveTab(GetDlgItem(window, DLG_BATTLELIST));
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
	case WM_NOTIFY: {
		NMHDR *info = (void *)lParam;
		if (info->idFrom == DLG_TOOLBAR && info->code == TBN_DROPDOWN) {
			NMTOOLBAR *info = (void *)lParam;
			HMENU menu = CreatePopupMenu();
			
			switch (info->iItem) {
			case ID_CONNECT:
				AppendMenu(menu, 0, ID_CONNECT, GetConnectionState() ? L"Disconnect" : L"Connect");
				SetMenuDefaultItem(menu, ID_CONNECT, 0);
				AppendMenu(menu, MF_SEPARATOR, 0, NULL);
				AppendMenu(menu, 0, ID_LOGINBOX, L"Login as a different user");
				// #ifndef NDEBUG
				AppendMenu(menu, 0, ID_SERVERLOG, L"Open Server Log");
				// #endif
				break;
			case ID_OPTIONS:
				AppendMenu(menu, 0, ID_LOBBY_PREFERENCES, L"Lobby Options");
				AppendMenu(menu, 0, ID_SPRING_SETTINGS, L"Spring Options");
				break;
			default:
				return 0;
			}
			
			ClientToScreen(window, (POINT *)&info->rcButton);
			TrackPopupMenuEx(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, info->rcButton.left, info->rcButton.top + info->rcButton.bottom, window, NULL);
			DestroyMenu(menu);
			return TBDDRET_DEFAULT;
		}
	}	break;
	case WM_COMMAND:
		switch (wParam) {
		case ID_CONNECT:
			if (GetConnectionState())
				Disconnect();
			else if (!Autologin())
				CreateLoginBox();
			return 0;
		case ID_LOGINBOX:
			CreateLoginBox();
			return 0;
		case ID_BATTLEROOM:
			MainWindow_SetActiveTab(gBattleRoomWindow);
			return 0;
		case ID_BATTLELIST:
			MainWindow_SetActiveTab(gBattleListWindow);
			return 0;
		case ID_DOWNLOADS:
			MainWindow_SetActiveTab(gDownloadTabWindow);
			return 0;
		case ID_SINGLEPLAYER:
			// JoinBattle(-1, NULL);
			return 0;
		case ID_REPLAY:
			return 0;
		case ID_HOSTBATTLE:
			CreateHostBattleDlg();
			return 0;
		case ID_OPTIONS:
			return 0;
		case ID_CHAT:
			MainWindow_SetActiveTab(gChatWindow);
			return 0;
		case ID_SERVERLOG:
			ChatWindow_SetActiveTab(GetServerChat());
			return 0;
		case ID_SPRING_SETTINGS:
			CreateProcess(L"springsettings.exe", L"springsettings.exe", NULL, NULL, 0, 0, NULL,NULL,
				&(STARTUPINFO){.cb=sizeof(STARTUPINFO)},
				&(PROCESS_INFORMATION){}
			);
			return 0;
		case ID_LOBBY_PREFERENCES:
			CreatePreferencesDlg();
			return 0;
		// case IDM_ABOUT:
			// CreateAboutDlg();
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
		// case IDM_RELOAD_MAPS_MODS:
			// ReloadMapsAndMod();
			// return 0;
		}
		break;
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

void MainWindow_ChangeConnect(enum ConnectionState state)
{
	SendDlgItemMessage(gMainWindow, DLG_TOOLBAR, TB_SETSTATE, ID_CHAT,
			state == CONNECTION_ONLINE ? TBSTATE_ENABLED : TBSTATE_DISABLED);
	SendDlgItemMessage(gMainWindow, DLG_TOOLBAR, TB_SETSTATE, ID_HOSTBATTLE,
			state == CONNECTION_ONLINE ? TBSTATE_ENABLED : TBSTATE_DISABLED);
	
	// SendDlgItemMessage(gMainWindow, DLG_TOOLBAR, TB_SETSTATE, tabIndex, state);
	// SendDlgItemMessage(gMainWindow, DLG_TOOLBAR, TB_SETSTATE, tabIndex, state);
	
	SendDlgItemMessage(gMainWindow, DLG_TOOLBAR, TB_SETBUTTONINFO, ID_CONNECT,
		(LPARAM)&(TBBUTTONINFO){
			.cbSize = sizeof(TBBUTTONINFO),
			.dwMask = TBIF_IMAGE | TBIF_TEXT,
			.iImage = (enum ICONS []){ICONS_OFFLINE, ICONS_CONNECTING, ICONS_ONLINE}[state],
			.pszText = (wchar_t *)(const wchar_t * []){L"Offline", L"Logging in", L"Online"}[state],
		});
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	InitSettings();
	
	
	srand(time(NULL));

	InitializeSystemMetrics();
	LoadLibrary(L"Riched20.dll");
	
	RegisterClassEx(&(WNDCLASSEX) {
		.cbSize = sizeof(WNDCLASSEX),
		.lpszClassName = WC_ALPHALOBBY,
		.lpfnWndProc	= winMainProc,
		.hIcon          = ImageList_GetIcon(gIconList, 0, 0),
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	});

	LONG left = CW_USEDEFAULT, top = CW_USEDEFAULT, width = CW_USEDEFAULT, height = CW_USEDEFAULT;
	const char *windowPlacement = LoadSetting("window_placement");
	if (windowPlacement)
		sscanf(windowPlacement, "%ld,%ld,%ld,%ld", &left, &top, &width, &height);

	Sync_Init();
	CreateWindowEx(0, WC_ALPHALOBBY, L"AlphaLobby", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		left, top, width, height,
		NULL, (HMENU)0, NULL, NULL);
	
	GetServerChat();
	Downloader_Init();
	/* CreateRapidDlg(); */
	
	char username[MAX_NAME_LENGTH_NUL], *s;
	if (gSettings.flags & SETTING_AUTOCONNECT
			&& (s = LoadSetting("username")) && strcpy(username, s)
			&& (s = LoadSetting("password")))
		Login(username, s);

	#ifndef NDEBUG
	if ((s = LoadSetting("last_map")))
		ChangedMap(strdup(s));
	if ((s = LoadSetting("last_mod")))
		ChangedMod(strdup(s));
	gBattleOptions.startPosType = STARTPOS_CHOOSE_INGAME;
	gBattleOptions.startRects[0] = (RECT){0, 0, 50, 200};
	gBattleOptions.startRects[1] = (RECT){150, 0, 200, 200};
	#endif
	
	
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
