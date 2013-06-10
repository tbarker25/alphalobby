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
#include <Commctrl.h>

#include "battle.h"
#include "battlelist.h"
#include "battleroom.h"
#include "channellist.h"
#include "chat.h"
#include "chat_window.h"
#include "client.h"
#include "client_message.h"
#include "common.h"
#include "dialogboxes.h"
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

#define WC_ALPHALOBBY L"AlphaLobby"

#ifndef NDEBUG
#define TBSTATE_DISABLED TBSTATE_ENABLED
#endif

HWND g_main_window;

static HWND current_tab;

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
	ID_CHANNEL,
	ID_PRIVATE,
	ID_LOBBY_PREFERENCES,
	ID_SPRING_SETTINGS,
	ID_ABOUT,
	ID_RESYNC,
};


static const DialogItem dialog_items[] = {
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

static void
resize_current_tab(int16_t width, int16_t height)
{
	RECT toolbar_rect;
	GetClientRect(GetDlgItem(g_main_window, DLG_TOOLBAR), &toolbar_rect);

	SetWindowPos(current_tab, NULL, 0, toolbar_rect.bottom, width, height - toolbar_rect.bottom, 0);
}

static void
set_current_tab_checked(char enable) {
	int tab_index = current_tab == g_battle_list   ? ID_BATTLELIST
		: current_tab == g_battle_room        ? ID_BATTLEROOM
		: current_tab == g_chat_window        ? ID_CHAT
		: current_tab == g_download_list ? ID_DOWNLOADS
		: -1;
	WPARAM state = SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_GETSTATE, tab_index, 0);
	if (enable)
		state |= TBSTATE_CHECKED;
	else
		state &= ~TBSTATE_CHECKED;
	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE, tab_index, state);
	ShowWindow(current_tab, enable);
}

void
MainWindow_set_active_tab(HWND new_tab)
{
	if (new_tab == current_tab)
		return;

	set_current_tab_checked(0);
	current_tab = new_tab;
	set_current_tab_checked(1);

	RECT rect;
	GetClientRect(g_main_window, &rect);
	resize_current_tab(rect.right, rect.bottom);
}

void
MainWindow_ring(void)
{
	FLASHWINFO flashInfo = {
		.cbSize = sizeof(FLASHWINFO),
		.hwnd = g_main_window,
		.dwFlags = 0x00000003 | /* FLASHW_ALL */
			0x0000000C, /* FLASHW_TIMERNOFG */
	};
	FlashWindowEx(&flashInfo);
	MainWindow_set_active_tab(g_battle_room);
}

void
MainWindow_disable_battleroom_button(void)
{
	if (current_tab == g_battle_room)
		MainWindow_set_active_tab(g_battle_list);
	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE, ID_BATTLEROOM, TBSTATE_DISABLED);
}

void
MainWindow_enable_battleroom_button(void)
{
	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE, ID_BATTLEROOM, TBSTATE_ENABLED | TBSTATE_CHECKED);
	MainWindow_set_active_tab(g_battle_room);
}

static const TBBUTTON tb_buttons[] = {
	{ ICONS_OFFLINE,     ID_CONNECT,      TBSTATE_ENABLED, BTNS_DROPDOWN, {}, 0, (INT_PTR)L"Offline" },
	{ I_IMAGENONE,       0,               TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_SEP, {}, 0, 0},
	{ ICONS_BATTLELIST,  ID_BATTLELIST,   TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Battle List"},

	{ ICONS_BATTLEROOM,  ID_BATTLEROOM,   TBSTATE_DISABLED,BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Battle Room"},
#ifndef NDEBUG
	{ ICONS_SINGLEPLAYER,ID_SINGLEPLAYER, TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Single player"},
	{ ICONS_REPLAY,      ID_REPLAY,       TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Replays"},
	{ ICONS_HOSTBATTLE,  ID_HOSTBATTLE,   TBSTATE_DISABLED,BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Host Battle"},
#endif
	{ ICONS_CHAT,        ID_CHAT,         TBSTATE_DISABLED,BTNS_DROPDOWN | BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Chat"},
	// { I_IMAGENONE,       ID_CHAT,      TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_WHOLEDROPDOWN, {}, 0, (INT_PTR)L"Users"},
	{ ICONS_OPTIONS,     ID_OPTIONS,      TBSTATE_ENABLED, BTNS_AUTOSIZE | BTNS_WHOLEDROPDOWN, {}, 0, (INT_PTR)L"Options"},
	{ ICONS_DOWNLOADS,   ID_DOWNLOADS,    TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"Downloads"},
};

static void
on_destroy()
{
	char window_placement_text[128];
	WINDOWPLACEMENT window_placement;
	RECT *r;

	Server_disconnect();
	Chat_save_windows();
	window_placement.length = sizeof(window_placement);
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
	g_main_window = window;
	CreateDlgItems(window, dialog_items, DLG_LAST+1);

	HWND toolbar = GetDlgItem(window, DLG_TOOLBAR);
	SendMessage(toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
	SendMessage(toolbar, TB_SETIMAGELIST, 0, (LPARAM)g_icon_list);

	SendMessage(toolbar, TB_BUTTONSTRUCTSIZE, sizeof(*tb_buttons), 0);
	SendMessage(toolbar, TB_ADDBUTTONS,       sizeof(tb_buttons) / sizeof(*tb_buttons), (LPARAM)&tb_buttons);
	SendMessage(toolbar, TB_AUTOSIZE, 0, 0);

	MainWindow_set_active_tab(GetDlgItem(window, DLG_BATTLELIST));
}

static LRESULT
on_command(int dialog_id)
{
	switch (dialog_id) {
	case ID_CONNECT:
		if (Server_status())
			Server_disconnect();
		else if (!Autologin())
			CreateLoginBox();
		return 0;

	case ID_LOGINBOX:
		CreateLoginBox();
		return 0;

	case ID_BATTLEROOM:
		MainWindow_set_active_tab(g_battle_room);
		return 0;

	case ID_BATTLELIST:
		MainWindow_set_active_tab(g_battle_list);
		return 0;

	case ID_DOWNLOADS:
		MainWindow_set_active_tab(g_download_list);
		return 0;

		/* case ID_SINGLEPLAYER: */

	case ID_REPLAY:
		return 0;

	case ID_HOSTBATTLE:
		CreateHostBattleDlg();
		return 0;

	case ID_CHAT:
		MainWindow_set_active_tab(g_chat_window);
		return 0;

	case ID_PRIVATE:
		UserList_show();
		return 0;

	case ID_CHANNEL:
		ChannelList_show();
		return 0;

	case ID_SERVERLOG:
		ChatWindow_set_active_tab(Chat_get_server_window());
		return 0;

	case ID_SPRING_SETTINGS:
		CreateProcess(L"springsettings.exe", L"springsettings.exe",
				NULL, NULL, 0, 0, NULL,NULL,
				&(STARTUPINFO){.cb=sizeof(STARTUPINFO)},
				&(PROCESS_INFORMATION){}
			     );
		return 0;

	case ID_LOBBY_PREFERENCES:
		CreatePreferencesDlg();
		return 0;

	case ID_ABOUT:
		CreateAboutDlg();
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
		// if (!GetTextDlg("Change username", name, MAX_NAME_LENGTH_NUL))
		// RenameAccount(name);
		// } return 0;
		// case IDM_CHANGE_PASSWORD:
		// CreateChange_password_dlg();
		// return 0;
	}
	return 1;
}

static LRESULT
create_dropdown(NMTOOLBAR *info)
{
	HMENU menu = CreatePopupMenu();

	switch (info->iItem) {
	case ID_CONNECT:
		AppendMenu(menu, 0, ID_CONNECT, Server_status() ? L"Server_disconnect" : L"Server_connect");
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

static LRESULT CALLBACK
main_window_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
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
		Server_poll();
		return 0;

	case WM_EXECFUNC:
		((void (*)(void))w_param)();
		return 0;

	case WM_EXECFUNCPARAM:
		((void (*)(LPARAM))w_param)(l_param);
		return 0;

	case WM_CREATE_DLG_ITEM:
		return (LRESULT)CreateDlgItem(window, (void *)l_param, w_param);

	case WM_TIMER:
		if (w_param == 1) {
			// Server_ping();
			return 0;
		}
		break;
	}
	return DefWindowProc(window, msg, w_param, l_param);
}

void
MainWindow_msg_box(const char *caption, const char *text)
{
	PostMessage(g_main_window, WM_MAKE_MESSAGEBOX, (WPARAM)_strdup(text), (WPARAM)_strdup(caption));
}

void
MainWindow_change_connect(enum ServerStatus state)
{
	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE, ID_CHAT,
			state == CONNECTION_ONLINE ? TBSTATE_ENABLED : TBSTATE_DISABLED);
	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE, ID_HOSTBATTLE,
			state == CONNECTION_ONLINE ? TBSTATE_ENABLED : TBSTATE_DISABLED);

	// SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE, tab_index, state);
	// SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETSTATE, tab_index, state);

	SendDlgItemMessage(g_main_window, DLG_TOOLBAR, TB_SETBUTTONINFO, ID_CONNECT,
			(LPARAM)&(TBBUTTONINFO){
			.cbSize = sizeof(TBBUTTONINFO),
			.dwMask = TBIF_IMAGE | TBIF_TEXT,
			.iImage = (enum ICONS []){ICONS_OFFLINE, ICONS_CONNECTING, ICONS_ONLINE}[state],
			.pszText = (wchar_t *)(const wchar_t * []){L"Offline", L"Logging in", L"Online"}[state],
			});
}

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrev_instance, LPSTR lp_cmd_line, int n_cmd_show)
{
	Settings_init();

	srand(time(NULL));

	/* InitializeSystemMetrics(); */
	LoadLibrary(L"Riched20.dll");

	RegisterClassEx(&(WNDCLASSEX) {
			.cbSize = sizeof(WNDCLASSEX),
			.lpszClassName = WC_ALPHALOBBY,
			.lpfnWndProc	= main_window_proc,
			.hIcon          = ImageList_GetIcon(g_icon_list, 0, 0),
			.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
			.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
			});

	LONG left = CW_USEDEFAULT, top = CW_USEDEFAULT, width = CW_USEDEFAULT, height = CW_USEDEFAULT;
	const char *window_placement = Settings_load_str("window_placement");
	if (window_placement)
		sscanf(window_placement, "%ld,%ld,%ld,%ld", &left, &top, &width, &height);

	Sync_init();
	CreateWindowEx(0, WC_ALPHALOBBY, L"AlphaLobby", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			left, top, width, height,
			NULL, (HMENU)0, NULL, NULL);

	Chat_get_server_window();
	Downloader_init();
	/* CreateRapidDlg(); */

	char username[MAX_NAME_LENGTH_NUL], *s;
	if (g_settings.flags & SETTING_AUTOCONNECT
			&& (s = Settings_load_str("username")) && strcpy(username, s)
			&& (s = Settings_load_str("password")))
		Login(username, s);

#ifndef NDEBUG
	if ((s = Settings_load_str("last_map")))
		Sync_on_changed_map(_strdup(s));
	if ((s = Settings_load_str("last_mod")))
		Sync_on_changed_mod(_strdup(s));
	g_battle_options.start_pos_type = STARTPOS_CHOOSE_INGAME;
	g_battle_options.start_rects[0] = (StartRect){0, 0, 50, 200};
	g_battle_options.start_rects[1] = (StartRect){150, 0, 200, 200};
#endif

	for (MSG msg; GetMessage(&msg, NULL, 0, 0) > 0; ) {
		if (msg.message == WM_KEYDOWN && msg.wParam == VK_F1)
			CreateAboutDlg();
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
