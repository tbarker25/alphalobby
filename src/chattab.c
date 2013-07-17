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
#include <limits.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys\stat.h>

#include <windows.h>
#include <commctrl.h>

#include "battle.h"
#include "chatbox.h"
#include "chatlist.h"
#include "chattab.h"
#include "common.h"
#include "layoutmetrics.h"
#include "mainwindow.h"
#include "mybattle.h"
#include "resource.h"
#include "tasserver.h"
#include "user.h"
#include "wincommon.h"

#define LENGTH(x) (sizeof x / sizeof *x)
#define foreach(_x, _xs) \
	for (typeof(&*_xs) _x = _xs; _x < _xs + sizeof _xs / sizeof *_xs; ++_x)

enum DialogId {
	DLG_TAB,
	DLG_LAST = DLG_TAB,
};

typedef struct TabItem {
	struct TabItem *next;
	HWND chat_window;
	HWND list_window;
	char name[];
} TabItem;

static void              _init             (void);
static intptr_t CALLBACK chat_window_proc  (HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);
static void              resize_active_tab (void);

static HWND s_tab_control;
static HWND s_chat_window;

static TabItem *private_tabs;
static TabItem *channel_tabs;
static TabItem *s_active_tab;

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
	int list_width;

	GetClientRect(s_tab_control, &rect);
	TabCtrl_AdjustRect(s_tab_control, FALSE, &rect);
	if (!s_active_tab)
		return;

	list_width = MAP_X(100);

	MoveWindow(s_active_tab->chat_window, rect.left, rect.top,
	    rect.right - rect.left - list_width, rect.bottom - rect.top, 1);
	MoveWindow(s_active_tab->list_window, rect.right - list_width,
	    rect.top, list_width, rect.bottom - rect.top, 1);
}

static void
add_tab(TabItem *item)
{
	wchar_t window_title[MAX_NAME_LENGTH_NUL];
	TCITEM info;

	GetWindowText(item->chat_window, window_title, MAX_NAME_LENGTH_NUL);

	info.mask = TCIF_TEXT | TCIF_PARAM;
	info.pszText = window_title;
	info.lParam = (intptr_t)item;

	SendMessage(s_tab_control, TCM_INSERTITEM, INT_MAX, (intptr_t)&info);
}

#if 0
static void
remove_tab(HWND tab_item)
{
	int index = get_tab_index(tab_item);
	TabCtrl_DeleteItem(s_tab_control, index);

	if (tab_item != s_active_tab)
		return;

	ShowWindow(tab_item, 0);

	if (index > 0)
		--index;

	TCITEM info;
	info.mask = TCIF_PARAM;
	TabCtrl_GetItem(s_tab_control, index, &info);

	s_active_tab = (HWND)info.lParam;
	ShowWindow(s_active_tab, 1);
	TabCtrl_SetCurSel(s_tab_control, index);
}
#endif

static int
get_tab_index(TabItem *tab_item)
{
	int item_len;

	item_len = SendMessage(s_tab_control, TCM_GETITEMCOUNT, 0, 0);

	for (int i=0; i<item_len; ++i) {
		TCITEM info;

		info.mask = TCIF_PARAM;
		SendMessage(s_tab_control, TCM_GETITEM, (uintptr_t)i,
		    (intptr_t)&info);
		if (tab_item == (TabItem *)info.lParam)
			return i;
	}
	return -1;
}

static void
focus_tab(TabItem *tab_item)
{
	int index;

	index = get_tab_index(tab_item);
	assert(index >= 0);

	TabCtrl_SetCurSel(s_tab_control, index);
	if (s_active_tab) {
		ShowWindow(s_active_tab->chat_window, 0);
		ShowWindow(s_active_tab->list_window, 0);
	}
	s_active_tab = tab_item;
	/* s_active_tab->chat_window = tab; */
	ShowWindow(tab_item->chat_window, 1);
	ShowWindow(tab_item->list_window, 1);
	resize_active_tab();
	MainWindow_set_active_tab(s_chat_window);
}

static intptr_t CALLBACK
chat_window_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	switch (msg) {
	case WM_CREATE:
		s_chat_window = window;
		CreateDlgItems(window, DIALOG_ITEMS, DLG_LAST + 1);
		s_tab_control = GetDlgItem(window, DLG_TAB);
		return 0;

	case WM_SIZE:
		SetWindowPos(s_tab_control, HWND_BOTTOM, 0, 0,
				LOWORD(l_param), HIWORD(l_param), SWP_NOMOVE);
		resize_active_tab();
		break;

	case WM_CLOSE:
		ShowWindow(window, 0);
		return 0;

	case WM_NOTIFY: {
		NMHDR *note;
		int tab_index;
		TCITEM info;

		note = (void *)l_param;
		if (note->idFrom != DLG_TAB || note->code != TCN_SELCHANGE)
			break;
		tab_index = TabCtrl_GetCurSel(note->hwndFrom);
		if (tab_index < 0)
			return 0;
		info.mask = TCIF_PARAM;
		SendMessage(s_tab_control, TCM_GETITEM, (uintptr_t)tab_index,
		    (intptr_t)&info);
		focus_tab((TabItem *)info.lParam);
		return 0;
	}
	}
	return DefWindowProc(window, msg, w_param, l_param);
}

static void __attribute__((constructor))
_init(void)
{
	WNDCLASS window_class = {
		.lpszClassName = WC_CHATTAB,
		.lpfnWndProc   = (WNDPROC)chat_window_proc,
		.hCursor       = LoadCursor(NULL, (wchar_t *)IDC_ARROW),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
	};

	RegisterClass(&window_class);
}

static TabItem *
get_user_window(User *user)
{
	TabItem **tab_item;

	tab_item = &private_tabs;
	while (*tab_item) {
		if (!strcmp((*tab_item)->name, user->name))
			return *tab_item;
		tab_item = &(*tab_item)->next;
	}

	*tab_item = calloc(1, strlen(user->name) + 1 + sizeof **tab_item);

	(*tab_item)->chat_window = CreateWindow(WC_CHATBOX,
	    utf8to16(user->name), WS_CHILD,
	    0, 0, 0, 0,
	    s_tab_control, 0, NULL, NULL);

	strcpy((*tab_item)->name, user->name);
	add_tab(*tab_item);

	ChatBox_set_say_function((*tab_item)->chat_window,
	    (SayFunction *)TasServer_send_say_private, NULL, user->name);
	return *tab_item;
}

void
ChatTab_on_said_private(User *user, const char *text, ChatType chat_type)
{
	ChatBox_append(get_user_window(user)->chat_window,
	    chat_type == CHAT_SELF ? g_my_user.name : user->name,
	    chat_type, text);
}

static TabItem *
get_channel_window(const char *channel)
{
	TabItem **tab_item;

	for (tab_item = &channel_tabs; *tab_item; tab_item = &(*tab_item)->next) {
		if (!strcmp(channel, (*tab_item)->name))
			return *tab_item;
	}

	*tab_item = calloc(1, strlen(channel) + 1 + sizeof **tab_item);

	(*tab_item)->chat_window = CreateWindow(WC_CHATBOX,
	    utf8to16(channel), WS_CHILD,
	    0, 0, 0, 0,
	    s_tab_control, 0, NULL, NULL);

	(*tab_item)->list_window = CreateWindow(WC_CHATLIST, L"TestWindow", WS_CHILD,
	    0, 0, 0, 0,
	    s_tab_control, 0, NULL, NULL);

	strcpy((*tab_item)->name, channel);

	add_tab(*tab_item);

	ChatBox_set_say_function((*tab_item)->chat_window,
	    (SayFunction *)TasServer_send_say_channel,
	    TasServer_send_leave_channel, channel);

	return *tab_item;
}

void
ChatTab_on_said_channel(const char *channel, struct User *user,
    const char *text, enum ChatType chat_type)
{
	TabItem *tab_item;

	tab_item = get_channel_window(channel);
	ChatBox_append(tab_item->chat_window, user->name, chat_type, text);
}

void
ChatTab_focus_channel(const char *channel)
{
	TabItem *tab_item;

	tab_item = get_channel_window(channel);
	focus_tab(tab_item);
}

void
ChatTab_focus_private(User *user)
{
	TabItem *tab_item;

	tab_item = get_user_window(user);
	focus_tab(tab_item);
}

void
ChatTab_add_user_to_channel(const char *channel, const User *user)
{
	TabItem *tab_item;

	tab_item = get_channel_window(channel);
	ChatList_add_user(tab_item->list_window, user);
}

void
ChatTab_remove_user_from_channel(const char *channel, const User *user)
{
	TabItem *tab_item;

	tab_item = get_channel_window(channel);
	ChatList_remove_user(tab_item->list_window, user);
}

#if 0
void
Chat_save_windows(void)
{
	char autojoin_channels[10000];
	autojoin_channels[0] = 0;
	size_t len = 0;
	for (size_t i=0; i<LENGTH(s_channel_windows); ++i) {
		extern int get_tab_index(HWND tab_item);
		if (get_tab_index(s_channel_windows[i]) >= 0) {
			ChatWindowData *data = (void *)GetWindowLongPtr(
					s_channel_windows[i], GWLP_USERDATA);
			len += sprintf(&autojoin_channels[len],
					len ? ";%s" : "%s", data->name);
		}
	}
	free(g_settings.autojoin);
	g_settings.autojoin = _strdup(autojoin_channels);
}
#endif

void
ChatTab_join_channel(const char *channel_name)
{
	for (const char *c=channel_name; *c; ++c) {
		if (!isalnum(*c) && *c != '[' && *c != ']') {
			MainWindow_msg_box("Couldn't join channel", "Unicode channels are not allowed.");
			return;
		}
	}

	TasServer_send_join_channel(channel_name);
	focus_tab(get_channel_window(channel_name));
}
