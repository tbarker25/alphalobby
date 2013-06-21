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
#include <stdlib.h>

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <basetyps.h>
#include <shellapi.h>

#include "battle.h"
#include "chatbox.h"
#include "chatlist.h"
#include "chattab.h"
#include "tasserver.h"
#include "common.h"
#include "downloader.h"
#include "iconlist.h"
#include "layoutmetrics.h"
#include "mainwindow.h"
#include "mybattle.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "user.h"
#include "wincommon.h"
#include "dialogs/gettextdialog.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

#define MARGIN (MAP_X(115))

enum {
	DLG_LOG,
	DLG_INPUT,
	DLG_LAST = DLG_INPUT,
};

#define ALIAS(c)\
	RGB(128 + GetRValue(c) / 2, 128 + GetGValue(c) / 2, 128 + GetBValue(c) / 2)
#define COLOR_TIMESTAMP RGB(200,200,200)

typedef struct WindowData {
	const char *dest;
	void (*func)(const char *dest, const char *text);

	int last_index, end, last_pos, offset;
	wchar_t buf[8192], *input_hint, *buffTail;
} WindowData;

static void             _init             (void);
static void             put_text          (HWND window, const char *text, COLORREF color, DWORD effects);
static LRESULT CALLBACK chat_box_proc     (HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
static void             on_create         (HWND window, LPARAM l_param);
static void             on_size           (HWND window, int width, int height);
static LRESULT CALLBACK log_proc          (HWND window, UINT msg, WPARAM w_param, LPARAM l_param, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
static LRESULT CALLBACK input_box_proc    (HWND window, UINT msg, WPARAM w_param, LPARAM l_param, UINT_PTR type, WindowData *data);
static void             on_escape_command (char *s, UINT_PTR type, const wchar_t *dest_name, HWND window);
static void             on_tab            (HWND window, UINT_PTR type, WindowData *data);
static BOOL CALLBACK    enum_child_proc   (HWND window, LPARAM user);
static void             update_user       (HWND window, User *u, int index);

static const COLORREF CHAT_COLORS[CHAT_LAST+1] = {
	[CHAT_EX] = RGB(225,152,163),
	[CHAT_INGAME] = RGB(90,122,150),
	[CHAT_SELF] = RGB(50,170,230),
	[CHAT_SELFEX] = RGB(50,170,230),
	[CHAT_SYSTEM] = RGB(80,150,80),
	[CHAT_TOPIC] = RGB(150,130,80),

	[CHAT_SERVERIN] = RGB(153,190,215),
	[CHAT_SERVEROUT] = RGB(215,190,153),
};

static const DialogItem DIALOG_ITEMS[] = {
	[DLG_LOG] = {
		.class = RICHEDIT_CLASS,
		.ex_style = WS_EX_WINDOWEDGE,
		.style = WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
	}, [DLG_INPUT] = {
		.class = WC_EDIT,
		.ex_style = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | ES_AUTOHSCROLL,
	}
};

static void
on_tab(HWND window, UINT_PTR type, WindowData *data)
{
	char text[MAX_TEXT_MESSAGE_LENGTH];
	GetWindowTextA(window, text, LENGTH(text));

	if (data->last_pos != SendMessage(window, EM_GETSEL, 0, 0) || data->end < 0) {
		data->end = LOWORD(SendMessage(window, EM_GETSEL, 0, 0));
		data->offset = 0;
		data->last_index = 0;
	}
	int offset = data->offset;
	int end = data->end + offset;
	int start = data->end + offset;

	while (start - offset && !isspace(text[start - 1 - offset]))
		--start;

	text[end] = L'\0';

}

static void
on_escape_command(char *s, UINT_PTR type, const wchar_t *dest_name, HWND window)
{
	char *code = strsep(&s, " ");

	if (!strcmp(code, "me")) {
		const char *message = s + LENGTH("me ") - 1;
		switch (type) {

		case DEST_BATTLE:
			TasServer_send_say_battle_ex(message);
			break;

		case DEST_PRIVATE:
			TasServer_send_say_private((User *)utf16to8(dest_name), message);
			break;

		case DEST_CHANNEL:
			TasServer_send_say_channel_ex(utf16to8(dest_name), message);
			break;

		case DEST_SERVER:
			/* TODO */
			/* TasServer_send("%s", message); */
			break;
		}

	} else if (!strcmp(code, "resync")) {
		Sync_reload();

	} else if (!strcmp(code, "dlmap")) {
		DownloadMap(s);

	} else if (!strcmp(code, "start")) {
		Spring_launch();

	} else if (!strcmp(code, "msg") || !strcmp(code, "pm")) {
		char *username = strsep(&s, " ");
		User *u = Users_find(username);
		if (u) {
			TasServer_send_say_private(u, s);
			return;
			/* ChatWindow_set_active_tab(Chat_get_private_window(u)); */
		}
		char buf[128];
		sprintf(buf, "Could not send message: %s is not logged in.", username);
		ChatBox_append(GetParent(window), NULL, CHAT_SYSTEM, buf);

	} else if (!strcmp(code, "j") || !strcmp(code, "join")) {
		/* Chat_join_channel(s, 1); */

	} else if (!strcmp(code, "away")) {
		ClientStatus cs = g_last_client_status;
		cs.away = !cs.away;
		TasServer_send_my_client_status(cs);

	} else if (!strcmp(code, "ingametime")) {
		TasServer_send_get_ingame_time();

	} else if (!strcmp(code, "split")) {
		char *split_type = strsep(&s, " ");
		SplitType type = !strcmp(split_type, "h") ? SPLIT_HORZ
			: !strcmp(split_type, "v") ? SPLIT_VERT
			: !strcmp(split_type, "c1") ? SPLIT_CORNERS1
			: !strcmp(split_type, "c2") ? SPLIT_CORNERS2
			: SPLIT_LAST+1;
		if (type <= SPLIT_LAST)
			MyBattle_set_split(type, atoi(s));

	} else if (!strcmp(code, "ingame")) {
		ClientStatus cs = g_last_client_status;
		cs.ingame = !cs.ingame;
		TasServer_send_my_client_status(cs);
	}
}

static LRESULT CALLBACK
input_box_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param,
		UINT_PTR type, WindowData *data)
{
	if (msg != WM_KEYDOWN)
		goto done;

	if (w_param != VK_TAB)
		data->end = -1;

	switch (w_param) {
	case VK_TAB:
		on_tab(window, type, data);
		return 0;

	case VK_DOWN:
		if (!data->input_hint)
			return 0;

		while (++data->input_hint != data->buf
		    && data->input_hint != data->buffTail
		    && ((data->input_hint)[-1] || !*data->input_hint))
			if (data->input_hint >= data->buf+LENGTH(data->buf)-1)
				data->input_hint = data->buf - 1;

		SetWindowText(window, data->input_hint);
		return 0;

	case VK_UP: {
		if (!data->input_hint)
			return 0;
		while ((--data->input_hint != data->buf)
				&& data->input_hint != data->buffTail
				&& ((data->input_hint)[-1] || !*data->input_hint))
			if (data->input_hint < data->buf)
				data->input_hint = data->buf + LENGTH(data->buf) - 1;
		SetWindowText(window, data->input_hint);
	}	return 0;

	case '\r': {
		if (LENGTH(data->buf) - (data->buffTail - data->buf) < MAX_TEXT_MESSAGE_LENGTH) {
			data->buffTail = data->buf;
			memset(data->buffTail, (LENGTH(data->buf) - (data->buffTail - data->buf)) * sizeof(*data->buf), '\0');
		}
		size_t len = GetWindowTextW(window, data->buffTail, MAX_TEXT_MESSAGE_LENGTH);
		if (len <= 0)
			return 0;

		char text_a[len * 3];
		WideCharToMultiByte(CP_UTF8, 0, data->buffTail, -1, text_a, sizeof(text_a), NULL, NULL);

		SetWindowText(window, NULL);
		wchar_t dest_name[128];
		GetWindowText(GetParent(window), dest_name, sizeof(dest_name));
		if (text_a[0] == '/') {
			on_escape_command(text_a + 1, type, dest_name, window);

		} else if (data->dest) {
			data->func(data->dest, text_a);

		} else {
			((void (*)(const char *))data->func)(text_a);
		}

		SetWindowLongPtr(GetDlgItem(GetParent(window), DLG_LOG), GWLP_USERDATA, 0);
		data->buffTail += len+1;
		data->input_hint = data->buffTail;
	}	return 0;
	}
	done:
	return DefSubclassProc(window, msg, w_param, l_param);
}


static LRESULT CALLBACK
log_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param,
		__attribute__((unused)) UINT_PTR uIdSubclass,
		__attribute__((unused)) DWORD_PTR dwRefData)
{
	if (msg == WM_VSCROLL)
		SetWindowLongPtr(window, GWLP_USERDATA, GetTickCount());

	return DefSubclassProc(window, msg, w_param, l_param);
}

static void
on_size(HWND window, int width, int height)
{
	HWND input = GetDlgItem(window, DLG_INPUT);
	HWND log   = GetDlgItem(window, DLG_LOG);
	MoveWindow(log, 0, 0, width, height - MAP_Y(14), 1);
	MoveWindow(input, 0, height - MAP_Y(14), width, MAP_Y(14), 1);
}

static void
on_create(HWND window, LPARAM l_param)
{
	/* ChatWindowData *data = ((CREATESTRUCT *)l_param)->lpCreateParams ?: &battle_room_data; */
	/* SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)data); */
	/* if (data->name) */
		/* SetWindowTextA(window, data->name); */
	HWND log_window = CreateDlgItem(window, &DIALOG_ITEMS[DLG_LOG], DLG_LOG);
	SendMessage(log_window, EM_EXLIMITTEXT, 0, INT_MAX);
	SendMessage(log_window, EM_AUTOURLDETECT, TRUE, 0);
	SendMessage(log_window, EM_SETEVENTMASK, 0, ENM_LINK | ENM_MOUSEEVENTS | ENM_SCROLL);
	SetWindowSubclass(log_window, log_proc, 0, 0);

	CreateDlgItem(window, &DIALOG_ITEMS[DLG_INPUT], DLG_INPUT);
}

void
ChatBox_set_say_function(HWND window, void (*func)(const char *dest, const char *text), const char *dest)
{
	WindowData *data;

	data = calloc(1, sizeof(*data));
	data->dest     = dest;
	data->func     = func;
	data->buffTail = data->buf;

	SetWindowSubclass(GetDlgItem(window, DLG_INPUT),
	    (void *)input_box_proc, 0, (DWORD_PTR)data);
}

static int
on_message_filter(HWND window, MSGFILTER *info)
{
#if 0
	if (info->msg != WM_RBUTTONUP)
		return 0;

	WindowData *data = (void *)GetWindowLongPtr(window, GWLP_USERDATA);

	HMENU menu = CreatePopupMenu();
	AppendMenu(menu, 0, 1, L"Copy");
	AppendMenu(menu, 0, 2, L"Clear window");
	AppendMenu(menu, g_settings.flags & SETTING_TIMESTAMP ? MF_CHECKED : 0, 3, L"Timestamp messages");

	/* if (data->type == DEST_PRIVATE) */
		/* AppendMenu(menu, 0, 6, L"Ignore user"); */

	/* AppendMenu(menu, g_settings.flags & (1<<data->type) ? MF_CHECKED : 0, 4, L"Show login/logout"); */

	/* AppendMenu(menu, 0, 5, (const wchar_t *[]){L"Leave Battle", L"Close private chat", L"Leave channel", L"Hide server log"}[data->type]); */

	POINT pt = {.x = LOWORD(info->lParam), .y = HIWORD(info->lParam)};
	ClientToScreen(info->nmhdr.hwndFrom, &pt);

	int index = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y, window, NULL);
	DestroyMenu(menu);
	switch (index) {
	case 1: {
		CHARRANGE info;
		SendMessage(note->hwndFrom, EM_EXGETSEL, 0, (LPARAM)&info);
		HGLOBAL *mem_buff = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * (info.cpMax - info.cpMin + 1));
		SendMessage(note->hwndFrom, EM_GETSELTEXT, 0, (LPARAM)GlobalLock(mem_buff));
		GlobalUnlock(mem_buff);
		OpenClipboard(window);
		EmptyClipboard();
		SetClipboardData(CF_UNICODETEXT, mem_buff);
		CloseClipboard();
		return 1;
	}
	case 2:
		SetWindowText(info->nmhdr.hwndFrom, NULL);
		return 1;
	case 3: case 4: {
		MENUITEMINFO info = {.cbSize = sizeof(MENUITEMINFO), .fMask = MIIM_STATE};
		GetMenuItemInfo(menu, index, 0, &info);
		int new_val = !(info.fState & MFS_CHECKED);
		if (index == 3)
			data->type = DEST_LAST + 1;
		g_settings.flags = (g_settings.flags & ~(1<<data->type)) | new_val<<data->type;
		return 1;
	}

	case 6: {
		User *u = Users_find(data->name);
		if (u)
			u->ignore = 1;
	}	// FALLTHROUGH:
	case 5:
		SendMessage(data->type == DEST_BATTLE ? GetParent(window) : window, WM_CLOSE, 0, 0);
		return 1;
	default:
		return 1;
	}
#endif
	return 1;
}

static int
on_link(ENLINK *link_info)
{
	wchar_t url[256];
	GETTEXTEX get_text_info;

	if (link_info->msg != WM_LBUTTONUP)
		return 0;

	SendMessage(link_info->nmhdr.hwndFrom, EM_EXSETSEL, 0,
	    (LPARAM)&link_info->chrg);

	get_text_info.cb       = sizeof(url);
	get_text_info.flags    = GT_SELECTION;
	get_text_info.codepage = 1200;

	SendMessage(link_info->nmhdr.hwndFrom, EM_GETTEXTEX,
	    (WPARAM)&get_text_info, (LPARAM)url);

	ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
	return 1;
}

static LRESULT CALLBACK
chat_box_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
    switch (msg) {
	case WM_CREATE:
		on_create(window, l_param);
		return 0;

	case WM_CLOSE: {
		/* ChatWindowData *data = (void *)GetWindowLongPtr(window, GWLP_USERDATA); */
		/* if (data->type == DEST_CHANNEL) */
			/* TasServer_send_leave_channel(data->name); */
		/* ChatWindow_remove_tab(window); */
		return 0;
	}
	case WM_SIZE:
		on_size(window, LOWORD(l_param), HIWORD(l_param));
		return 0;

	case WM_COMMAND:
		if (w_param == MAKEWPARAM(DLG_LOG, EN_VSCROLL))
			SetWindowLongPtr((HWND)l_param, GWLP_USERDATA, GetTickCount());
		return 0;

	case WM_NOTIFY: {
		NMHDR *note = (NMHDR *)l_param;

		if (note->idFrom == DLG_LOG && note->code == EN_MSGFILTER)
			return on_message_filter(window, (MSGFILTER *)l_param);

		if (note->idFrom == DLG_LOG && note->code == EN_LINK)
			return on_link((ENLINK *)l_param);

	}	break;
	}
	return DefWindowProc(window, msg, w_param, l_param);
}

static void
put_text(HWND window, const char *text, COLORREF color, DWORD effects)
{
	CHARFORMAT format;
	SETTEXTEX set_text_info;

	format.cbSize = sizeof(format);
	format.dwMask = CFM_BOLD | CFM_COLOR;
	format.dwEffects = effects;
	format.crTextColor = color;
	SendMessage(window, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);

	set_text_info.flags = ST_SELECTION;
	set_text_info.codepage = 65001;
	SendMessage(window, EM_SETTEXTEX, (WPARAM)&set_text_info, (LPARAM)text);
}

void
ChatBox_append(HWND window, const char *username, ChatType type, const char *text)
{
	COLORREF color;

	window = GetDlgItem(window, DLG_LOG);
	SendMessage(window, EM_EXSETSEL, 0, (LPARAM)&(CHARRANGE){INFINITE, INFINITE});

	if ((type != CHAT_TOPIC) && g_settings.flags & SETTING_TIMESTAMP) {
		char buf[128];
		char *s = buf;

		*(s++) = '[';
		s += GetTimeFormatA(0, 0, NULL, NULL, s, sizeof(buf) - 2) - 1;
		*(s++) = ']';
		*(s++) = ' ';
		*s = '\0';
		put_text(window, buf, COLOR_TIMESTAMP, 0);
	}

	color = CHAT_COLORS[type];
	if (type == CHAT_SERVERIN || type == CHAT_SERVEROUT)
		put_text(window, type == CHAT_SERVERIN ? "> " : "< ", color, 0);

	if (username) {
		const User *u;
		const char *alias;

		put_text(window, username, color, !(type & CHAT_SYSTEM) * CFE_BOLD);

		u = Users_find(username);
		alias = u ? u->alias : "logged out";
		if (strcmp(alias, UNTAGGED_NAME(username))) {
			char buf[128];

			sprintf(buf, " (%s)", alias);
			put_text(window, buf, ALIAS(color), 0);
		}
		put_text(window, type & (CHAT_EX | CHAT_SYSTEM) ? " " : ": ",
		    color, type & CHAT_SYSTEM ? 0 : CFE_BOLD);
	}

	put_text(window, text, color, 0);
	put_text(window, "\n", color, 0);

	if (GetTickCount() - GetWindowLongPtr(window, GWLP_USERDATA) > 5000) {
		SendMessage(window, WM_VSCROLL, SB_BOTTOM, 0);
		SetWindowLongPtr(window, GWLP_USERDATA, 0);
	}
}

static void __attribute__((constructor))
_init(void)
{
	WNDCLASS window_class = {
		.lpszClassName = WC_CHATBOX,
		.lpfnWndProc   = chat_box_proc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};

	RegisterClass(&window_class);
}
