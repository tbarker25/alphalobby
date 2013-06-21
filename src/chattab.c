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
#include <ctype.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys\stat.h>

#include <windows.h>
#include <commctrl.h>

#include "battle.h"
#include "tasserver.h"
#include "chatbox.h"
#include "chattab.h"
#include "common.h"
#include "mainwindow.h"
#include "mybattle.h"
#include "resource.h"
#include "user.h"
#include "wincommon.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

enum DialogId {
	DLG_TAB,
	DLG_LAST = DLG_TAB,
};

static void             _init             (void);
static LRESULT CALLBACK chat_window_proc  (HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
static void             resize_active_tab (void);

static HWND g_chat_window;
static HWND g_tab_control;
static HWND active_tab;

static HWND g_channel_windows[128];

static const DialogItem DIALOG_ITEMS[] = {
	[DLG_TAB] = {
		.class = WC_TABCONTROL,
		.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
	}
};

static void
resize_active_tab(void)
{
	RECT rect;

	GetClientRect(g_tab_control, &rect);
	TabCtrl_AdjustRect(g_tab_control, FALSE, &rect);
	MoveWindow(active_tab, rect.left, rect.top, rect.right - rect.left,
			rect.bottom - rect.top, 1);
}

static int
get_tab_index(HWND tab_item)
{
	int tab_len;

	tab_len = TabCtrl_GetItemCount(g_tab_control);
	for (int i=0; i<tab_len; ++i) {
		TCITEM info;

		info.mask = TCIF_PARAM;
		TabCtrl_GetItem(g_tab_control, i, &info);
		if ((HWND)info.lParam == tab_item)
			return i;
	}
	return -1;
}

static int
add_tab(HWND tab)
{
	wchar_t window_title[MAX_NAME_LENGTH_NUL];
	int item_index;

	item_index = get_tab_index(tab);
	if  (item_index >= 0)
		return item_index;

	GetWindowText(tab, window_title, MAX_NAME_LENGTH_NUL);

	TCITEM info;
	info.mask = TCIF_TEXT | TCIF_PARAM;
	info.pszText = window_title;
	info.lParam = (LPARAM)tab;

	item_index = TabCtrl_InsertItem(g_tab_control, 1000, &info);
	if (item_index == 0) {
		active_tab = tab;
		ShowWindow(tab, 1);
		resize_active_tab();
	}

	return item_index;
}

#if 0
static void
remove_tab(HWND tab_item)
{
	int index = get_tab_index(tab_item);
	TabCtrl_DeleteItem(g_tab_control, index);

	if (tab_item != active_tab)
		return;

	ShowWindow(tab_item, 0);

	if (index > 0)
		--index;

	TCITEM info;
	info.mask = TCIF_PARAM;
	TabCtrl_GetItem(g_tab_control, index, &info);

	active_tab = (HWND)info.lParam;
	ShowWindow(active_tab, 1);
	TabCtrl_SetCurSel(g_tab_control, index);
}
#endif

static void
focus_tab(HWND tab)
{
	int item_index;

	item_index = add_tab(tab);
	TabCtrl_SetCurSel(g_tab_control, item_index);
	ShowWindow(active_tab, 0);
	active_tab = tab;
	ShowWindow(active_tab, 1);
	resize_active_tab();
	MainWindow_set_active_tab(g_chat_window);
}

static void say_hello(const char *text, bool unused, const void *dest)
{
	printf("hello %s from %s\n", text, (char *)dest);
}

static void doit(void)
{
	HWND chat_window = CreateWindow(WC_CHATBOX, L"TestWindow", WS_CHILD,
	    0, 0, 0, 0,
	    g_tab_control, 0, NULL, NULL);
	focus_tab(chat_window);
	ChatBox_set_say_function(chat_window, say_hello, "blah");
}


static LRESULT CALLBACK
chat_window_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg) {
	case WM_CREATE:
		g_chat_window = window;

		CreateDlgItems(window, DIALOG_ITEMS, DLG_LAST + 1);
		g_tab_control = GetDlgItem(window, DLG_TAB);
		doit();
		return 0;

	case WM_SIZE:
		SetWindowPos(g_tab_control, HWND_BOTTOM, 0, 0,
				LOWORD(l_param), HIWORD(l_param), SWP_NOMOVE);
		resize_active_tab();
		break;

	case WM_CLOSE:
		ShowWindow(window, 0);
		return 0;

	case WM_NOTIFY: {
		NMHDR *info = (void *)l_param;

		if (info->hwndFrom == g_tab_control && info->code == TCN_SELCHANGE) {
			int tab_index = TabCtrl_GetCurSel(info->hwndFrom);
			TCITEM info;
			info.mask = TCIF_PARAM;
			TabCtrl_GetItem(g_tab_control, tab_index, &info);
			focus_tab((HWND)info.lParam);
			return 0;
		}
	}	break;
	}
	return DefWindowProc(window, msg, w_param, l_param);
}

static void __attribute__((constructor))
_init(void)
{	WNDCLASS window_class = {
		.lpszClassName = WC_CHATTAB,
		.lpfnWndProc   = chat_window_proc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};

	RegisterClass(&window_class);
}

static HWND get_user_window(User *user)
{
	HWND chat_window;

	if (user->chat_window)
		return user->chat_window;
	chat_window = CreateWindow(WC_CHATBOX,
	    utf8to16(user->name), WS_CHILD,
	    0, 0, 0, 0,
	    g_tab_control, 0, NULL, NULL);
	focus_tab(chat_window);
	ChatBox_set_say_function(chat_window,
	    (SayFunction *)TasServer_send_say_private, user->name);
	user->chat_window = chat_window;
	return chat_window;
}

void
ChatTab_on_said_private(User *user, const char *text, ChatType chat_type)
{
	HWND user_window;

	user_window = get_user_window(user);
	ChatBox_append(user_window,
	    chat_type == CHAT_SELF ? g_my_user.name : user->name,
	    chat_type, text);
}

static HWND
get_channel_window(const char *channel)
{
	for (size_t i = 0; i < LENGTH(g_channel_windows); ++i) {
		HWND channel_window;
		char buf[128];

		if (g_channel_windows[i]) {
			GetWindowTextA(g_channel_windows[i], buf, sizeof(buf));
			if (!strcmp(channel, buf))
				return g_channel_windows[i];
		}
		channel_window = CreateWindow(WC_CHATBOX,
		    utf8to16(channel), WS_CHILD,
		    0, 0, 0, 0,
		    g_tab_control, 0, NULL, NULL);
		g_channel_windows[i] = channel_window;
		focus_tab(channel_window);
		/* TODO mem leak here */
		ChatBox_set_say_function(channel_window,
		    (SayFunction *)TasServer_send_say_channel, _strdup(channel));

		return channel_window;
	}
	assert(0);
	return NULL;
}

void
ChatTab_on_said_channel(const char *channel, struct User *user,
    const char *text, enum ChatType chat_type)
{
	HWND channel_window;

	channel_window = get_channel_window(channel);
	ChatBox_append(channel_window, user->name, chat_type, text);
}

void
ChatTab_focus_channel(const char *channel)
{
	HWND channel_window;

	channel_window = get_channel_window(channel);
	focus_tab(channel_window);
}

void
ChatTab_focus_private(struct User *user)
{
	HWND user_window;

	user_window = get_user_window(user);
	focus_tab(user_window);
}

#if 0
void
Chat_save_windows(void)
{
	char autojoin_channels[10000];
	autojoin_channels[0] = 0;
	size_t len = 0;
	for (size_t i=0; i<LENGTH(g_channel_windows); ++i) {
		extern int get_tab_index(HWND tab_item);
		if (get_tab_index(g_channel_windows[i]) >= 0) {
			ChatWindowData *data = (void *)GetWindowLongPtr(
					g_channel_windows[i], GWLP_USERDATA);
			len += sprintf(&autojoin_channels[len],
					len ? ";%s" : "%s", data->name);
		}
	}
	free(g_settings.autojoin);
	g_settings.autojoin = _strdup(autojoin_channels);
}

#endif
