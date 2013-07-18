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

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <windows.h>
#include <commctrl.h>

#include "battle.h"
#include "battlelist.h"
#include "battleroom.h"
#include "channellist.h"
#include "chatbox.h"
#include "chattab.h"
#include "tasserver.h"
#include "common.h"
#include "downloader.h"
#include "downloadtab.h"
#include "iconlist.h"
#include "layoutmetrics.h"
#include "mainwindow.h"
#include "mybattle.h"
#include "settings.h"
#include "sync.h"
#include "user.h"
#include "userlist.h"
#include "wincommon.h"
#include "dialogs/aboutdialog.h"
#include "dialogs/changepassworddialog.h"
#include "dialogs/hostdialog.h"
#include "dialogs/logindialog.h"
#include "dialogs/preferencesdialog.h"
#include "dialogs/replaydialog.h"

#define WC_ALPHALOBBY L"AlphaLobby"
#define LENGTH(x) (sizeof x / sizeof *x)

#ifndef NDEBUG
#define TBSTATE_DISABLED TBSTATE_ENABLED
#else
#define TBSTATE_DISABLED (0)
#endif

enum {
	DLG_TOOLBAR,
	DLG_BATTLELIST,
	DLG_BATTLEROOM,
	DLG_CHAT,
	DLG_DOWNLOADTAB,
	DLG_LAST = DLG_DOWNLOADTAB,
};

enum DialogId {
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
	ID_CHANNEL,
	ID_PRIVATE,
	ID_LOBBY_PREFERENCES,
	ID_SPRING_SETTINGS,
	ID_ABOUT,
	ID_RESYNC,
};

static void     _init              (void);
static bool     autologin          (void);
static intptr_t CALLBACK main_window_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);
static intptr_t create_dropdown    (NMTOOLBAR *info);
static void     launch_spring_settings(void);
static intptr_t on_command         (enum DialogId dialog_id);
static void     on_create          (HWND window);
static void     on_destroy         (void);
static void     set_current_tab_checked(char enable);
static void     resize_current_tab (int16_t width, int16_t height);

HWND g_main_window;
static HWND s_current_tab;

static const DialogItem DIALOG_ITEMS[] = {
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
		.class = WC_CHATTAB,
	}, [DLG_DOWNLOADTAB] = {
		.class = WC_DOWNLOADTAB,
	}
};

static const TBBUTTON TOOLBAR_BUTTONS[] = {
	{ ICON_OFFLINE,     ID_CONNECT,      TBSTATE_ENABLED, BTNS_DROPDOWN, {}, 0, (intptr_t)L"Offline" },
	{ I_IMAGENONE,       0,               TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_SEP, {}, 0, 0},
	{ ICON_BATTLELIST,  ID_BATTLELIST,   TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (intptr_t)L"Battle List"},

	{ ICON_BATTLEROOM,  ID_BATTLEROOM,   TBSTATE_DISABLED,BTNS_AUTOSIZE, {}, 0, (intptr_t)L"Battle Room"},
#ifndef NDEBUG
	{ ICON_SINGLEPLAYER,ID_SINGLEPLAYER, TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (intptr_t)L"Single player"},
	{ ICON_REPLAY,      ID_REPLAY,       TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (intptr_t)L"Replays"},
	{ ICON_HOSTBATTLE,  ID_HOSTBATTLE,   TBSTATE_DISABLED,BTNS_AUTOSIZE, {}, 0, (intptr_t)L"Host Battle"},
#endif
	{ ICON_CHAT,        ID_CHAT,         TBSTATE_DISABLED,BTNS_DROPDOWN | BTNS_AUTOSIZE, {}, 0, (intptr_t)L"Chat"},
	{ ICON_OPTIONS,     ID_OPTIONS,      TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_WHOLEDROPDOWN, {}, 0, (intptr_t)L"Options"},
	{ ICON_DOWNLOADS,   ID_DOWNLOADS,    TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (intptr_t)L"Downloads"},
};

static void
resize_current_tab(int16_t width, int16_t height)
{
	RECT toolbar_rect;

	GetClientRect(GetDlgItem(g_main_window, DLG_TOOLBAR), &toolbar_rect);
	SetWindowPos(s_current_tab, NULL, 0, toolbar_rect.bottom, width, height - toolbar_rect.bottom, 0);
}

static void
set_current_tab_checked(char enable)
{
	int dialog_id;
	int tab_index;

	dialog_id = GetDlgCtrlID(s_current_tab);

	tab_index = dialog_id == DLG_BATTLELIST  ? ID_BATTLELIST
	          : dialog_id == DLG_BATTLEROOM  ? ID_BATTLEROOM
		  : dialog_id == DLG_CHAT        ? ID_CHAT
		  : dialog_id == DLG_DOWNLOADTAB ? ID_DOWNLOADS
		  : -1;

	uintptr_t state = (uintptr_t)SendDlgItemMessage(g_main_window, DLG_TOOLBAR,
	    TB_GETSTATE, (uintptr_t)tab_index, 0);

	if (enable) {
		state |= (uintptr_t)TBSTATE_CHECKED;
	} else {
		state &= ~(uintptr_t)TBSTATE_CHECKED;
	}

	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE,
	    (uintptr_t)tab_index, (intptr_t)state);
	ShowWindow(s_current_tab, enable);
}

void
MainWindow_set_active_tab(HWND new_tab)
{
	if (new_tab == s_current_tab)
		return;

	set_current_tab_checked(0);
	s_current_tab = new_tab;
	set_current_tab_checked(1);

	RECT rect;
	GetClientRect(g_main_window, &rect);
	resize_current_tab((int16_t)rect.right, (int16_t)rect.bottom);
}

void
MainWindow_ring(void)
{
	FLASHWINFO flash_info = {
		.cbSize = sizeof flash_info,
		.hwnd = g_main_window,
		.dwFlags = 0x00000003 | /* FLASHW_ALL */
			0x0000000C, /* FLASHW_TIMERNOFG */
	};

	FlashWindowEx(&flash_info);
	MainWindow_set_active_tab(GetDlgItem(g_main_window, DLG_BATTLEROOM));
}

void
MainWindow_disable_battleroom_button(void)
{
	if (GetDlgCtrlID(s_current_tab) == DLG_BATTLEROOM)
		MainWindow_set_active_tab(GetDlgItem(g_main_window, DLG_BATTLELIST));

	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE,
	    ID_BATTLEROOM, TBSTATE_DISABLED);
}

void
MainWindow_enable_battleroom_button(void)
{
	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE, ID_BATTLEROOM, TBSTATE_ENABLED | TBSTATE_CHECKED);
	MainWindow_set_active_tab(GetDlgItem(g_main_window, DLG_BATTLEROOM));
}

static void
on_destroy(void)
{
	char window_placement_text[128];
	WINDOWPLACEMENT window_placement;
	RECT *r;

	TasServer_disconnect();
	window_placement.length = sizeof window_placement;
	GetWindowPlacement(g_main_window, &window_placement);
	r = &window_placement.rcNormalPosition;
	r->bottom -= r->top;
	r->right -= r->left;
	if (window_placement.showCmd == SW_MAXIMIZE) {
		r->left = CW_USEDEFAULT;
		r->top = SW_SHOWMAXIMIZED;
	}
	sprintf(window_placement_text, "%ld,%ld,%ld,%ld", r->left, r->top, r->right, r->bottom);
	Settings_save_str("window_placement", window_placement_text);
	Sync_cleanup();
	Settings_save_aliases();
	PostQuitMessage(0);
}

static void
on_create(HWND window)
{
	HWND toolbar;

	g_main_window = window;
	CreateDlgItems(window, DIALOG_ITEMS, DLG_LAST + 1);

	toolbar = GetDlgItem(window, DLG_TOOLBAR);
	IconList_enable_for_toolbar(toolbar);
	SendMessage(toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
	SendMessage(toolbar, TB_BUTTONSTRUCTSIZE, sizeof *TOOLBAR_BUTTONS, 0);
	SendMessage(toolbar, TB_ADDBUTTONS, LENGTH(TOOLBAR_BUTTONS), (intptr_t)&TOOLBAR_BUTTONS);
	SendMessage(toolbar, TB_AUTOSIZE, 0, 0);

	MainWindow_set_active_tab(GetDlgItem(window, DLG_BATTLELIST));
}

static intptr_t
on_command(enum DialogId dialog_id)
{
	switch (dialog_id) {
	case ID_CONNECT:
		if (TasServer_status()) {
			TasServer_disconnect();

		} else if (!autologin()) {
			LoginDialog_create();
		}
		return 0;

	case ID_LOGINBOX:
		LoginDialog_create();
		return 0;

	case ID_BATTLEROOM:
		MainWindow_set_active_tab(GetDlgItem(g_main_window, DLG_BATTLEROOM));
		return 0;

	case ID_BATTLELIST:
		MainWindow_set_active_tab(GetDlgItem(g_main_window, DLG_BATTLELIST));
		return 0;

	case ID_DOWNLOADS:
		MainWindow_set_active_tab(GetDlgItem(g_main_window, DLG_DOWNLOADTAB));
		return 0;

		/* case ID_SINGLEPLAYER: */

	case ID_REPLAY:
		ReplayDialog_create();
		return 0;

	case ID_HOSTBATTLE:
		HostDialog_create();
		return 0;

	case ID_CHAT:
		MainWindow_set_active_tab(GetDlgItem(g_main_window, DLG_CHAT));
		return 0;

	case ID_PRIVATE:
		UserList_show();
		return 0;

	case ID_CHANNEL:
		ChannelList_show();
		return 0;

	case ID_SERVERLOG:
		/* ChatWindow_set_active_tab(Chat_get_server_window()); */
		return 0;

	case ID_SPRING_SETTINGS:
		launch_spring_settings();
		return 0;

	case ID_LOBBY_PREFERENCES:
		PreferencesDialog_create();
		return 0;

	case ID_ABOUT:
		AboutDialog_create();
		return 0;

	case ID_RESYNC:
		Sync_reload();
		return 0;

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
		// if (!GetTextDialog_create("Change username", name, MAX_NAME_LENGTH_NUL))
		// TasServer_send_rename(name);
		// } return 0;
	/* case ID_CHANGE_PASSWORD: */
		/* ChangePasswordDialog_create(); */
		/* return 0; */
	default:
		return 1;
	}
}

static intptr_t
create_dropdown(NMTOOLBAR *info)
{
	HMENU menu = CreatePopupMenu();

	switch (info->iItem) {
	case ID_CONNECT:
		AppendMenu(menu, 0, ID_CONNECT, TasServer_status() ? L"Disconnect" : L"Connect");
		SetMenuDefaultItem(menu, ID_CONNECT, 0);
		AppendMenu(menu, MF_SEPARATOR, 0, NULL);
		AppendMenu(menu, 0, ID_LOGINBOX, L"Login as a different user");
		AppendMenu(menu, 0, ID_SERVERLOG, L"Open server log");
		break;

	case ID_OPTIONS:
		AppendMenu(menu, 0, ID_LOBBY_PREFERENCES, L"Lobby options");
		AppendMenu(menu, 0, ID_SPRING_SETTINGS, L"Spring options");
		AppendMenu(menu, MF_SEPARATOR, 0, NULL);
		AppendMenu(menu, 0, ID_RESYNC, L"Reload maps and mod");
		AppendMenu(menu, 0, ID_ABOUT, L"About AlphaLobby");
		break;

	case ID_CHAT:
		AppendMenu(menu, 0, ID_CHAT, L"Open chat tab");
		SetMenuDefaultItem(menu, ID_CHAT, 0);
		AppendMenu(menu, MF_SEPARATOR, 0, NULL);
		AppendMenu(menu, 0, ID_CHANNEL, L"Open channel...");
		AppendMenu(menu, 0, ID_PRIVATE, L"Open private chat with...");
		AppendMenu(menu, 0, ID_SERVERLOG, L"Open server log");
		break;

	default:
		return 0;
	}

	ClientToScreen(g_main_window, (POINT *)&info->rcButton);
	TrackPopupMenuEx(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON,
			info->rcButton.left,
			info->rcButton.top + info->rcButton.bottom,
			g_main_window, NULL);

	DestroyMenu(menu);
	return TBDDRET_DEFAULT;
}

static intptr_t CALLBACK
main_window_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	switch(msg) {
	case WM_CREATE:
		on_create(window);
		return 0;

	case WM_DESTROY:
		on_destroy();
		return 0;

	case WM_SIZE:
		MoveWindow(GetDlgItem(window, DLG_TOOLBAR), 0, 0, LOWORD(l_param), 0, 0);
		resize_current_tab(LOWORD(l_param), HIWORD(l_param));
		return 0;

	case WM_NOTIFY:
		if (((NMHDR *)l_param)->idFrom == DLG_TOOLBAR
				&& ((NMHDR *)l_param)->code == TBN_DROPDOWN)
			return create_dropdown((NMTOOLBAR *)l_param);
		break;

	case WM_EXITSIZEMOVE:
		SendDlgItemMessage(window, DLG_BATTLELIST, WM_EXITSIZEMOVE, 0, 0);
		return 0;

	case WM_COMMAND:
		return on_command(w_param);

	case WM_MAKE_MESSAGEBOX:
		MessageBoxA(window, (char *)w_param, (char *)l_param, 0);
		free((void *)w_param);
		free((void *)l_param);
		return 0;

	case WM_DESTROY_WINDOW:
		DestroyWindow((HWND)l_param);
		return 0;

	case WM_POLL_SERVER:
		TasServer_poll();
		return 0;

	case WM_EXECFUNC:
		((void (*)(void))w_param)();
		return 0;

	case WM_EXECFUNCPARAM:
		((void (*)(intptr_t))w_param)(l_param);
		return 0;

	case WM_CREATE_DLG_ITEM:
		return (intptr_t)CreateDlgItem(window, (DialogItem *)l_param, w_param);
	}
	return DefWindowProc(window, msg, w_param, l_param);
}

void
MainWindow_msg_box(const char *caption, const char *text)
{
	PostMessage(g_main_window, WM_MAKE_MESSAGEBOX,
	    (uintptr_t)_strdup(text), (intptr_t)_strdup(caption));
}

void
MainWindow_change_connect(enum ServerStatus state)
{
	TBBUTTONINFO info;

	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE, ID_CHAT,
			state == CONNECTION_ONLINE ? TBSTATE_ENABLED : TBSTATE_DISABLED);
	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE, ID_HOSTBATTLE,
			state == CONNECTION_ONLINE ? TBSTATE_ENABLED : TBSTATE_DISABLED);

	info.cbSize = sizeof info;
	info.dwMask = TBIF_IMAGE | TBIF_TEXT;
	info.iImage = (IconIndex []){ICON_OFFLINE, ICON_CONNECTING, ICON_ONLINE}[state];
	info.pszText = (wchar_t *)(const wchar_t * []){L"Offline", L"Logging in", L"Online"}[state];

	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETBUTTONINFO,
			ID_CONNECT, (intptr_t)&info);
}

int WINAPI
WinMain(__attribute__((unused)) HINSTANCE instance,
		__attribute__((unused)) HINSTANCE prev_instance,
		__attribute__((unused)) PSTR cmd_line,
		__attribute__((unused)) int cmd_show)
{
	int32_t left;
	int32_t top;
	int32_t width;
	int32_t height;

	Settings_init();

	LoadLibrary(L"Riched20.dll");

	const char *window_placement = Settings_load_str("window_placement");
	if (!window_placement
	    || sscanf(window_placement, "%d,%d,%d,%d", &left, &top, &width, &height) != 4) {
		left = CW_USEDEFAULT;
		top = CW_USEDEFAULT;
		width = CW_USEDEFAULT;
		height = CW_USEDEFAULT;
	}

	Sync_init();
	CreateWindowEx(0, WC_ALPHALOBBY, L"AlphaLobby", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			left, top, width, height,
			NULL, (HMENU)0, NULL, NULL);

	Downloader_init();

	if (g_settings.flags & SETTING_AUTOCONNECT)
		autologin();

#ifndef NDEBUG
	char *s;
	if ((s = Settings_load_str("last_map")))
		Sync_on_changed_map(_strdup(s));
	if ((s = Settings_load_str("last_mod")))
		Sync_on_changed_mod(_strdup(s));
	g_battle_options.start_pos_type = STARTPOS_CHOOSE_INGAME;
	g_battle_options.start_rects[0] = (StartRect){0, 0, 50, 200};
	g_battle_options.start_rects[1] = (StartRect){150, 0, 200, 200};
#endif

	for (MSG msg; GetMessage(&msg, NULL, 0, 0) > 0; ) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

static bool
autologin(void)
{
	char username[MAX_NAME_LENGTH_NUL], password[1024];

	if (Settings_load_str2(username, "username"))
		return false;

	if (Settings_load_str2(password, "password"))
		return false;

	TasServer_send_login(username, password);
	return true;
}

static void __attribute__((constructor))
_init(void)
{
	WNDCLASSEX window_class = {
		.cbSize        = sizeof window_class,
		.lpszClassName = WC_ALPHALOBBY,
		.lpfnWndProc   = (WNDPROC)main_window_proc,
		.hIcon         = IconList_get_icon(ICON_ALPHALOBBY),
		.hCursor       = LoadCursor(NULL, (wchar_t *)IDC_ARROW),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
	};

	RegisterClassEx(&window_class);
}

static void
launch_spring_settings(void)
{
	STARTUPINFO startup_info = {0};
	PROCESS_INFORMATION process_info;

	startup_info.cb = sizeof startup_info;
	CreateProcess((wchar_t *)L"springsettings.exe",
	    (wchar_t *)L"springsettings.exe",
	    NULL, NULL, 0, 0, NULL,NULL,
	    &startup_info, &process_info);
}
