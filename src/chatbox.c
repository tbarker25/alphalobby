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

#define LENGTH(x) (sizeof x / sizeof *x)

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
	SayFunction *on_say;
	OnCloseFunction *on_close;
	const char *dst;

	uint32_t last_scroll_time;

	int last_index, end, last_pos, offset;
	wchar_t buf[8192], *input_hint, *buf_end;
} WindowData;

static intptr_t CALLBACK chat_box_proc     (HWND,   uint32_t msg, uintptr_t, intptr_t);
static void              copy_selected_to_clipboard (HWND);
static intptr_t CALLBACK input_box_proc    (HWND,   uint32_t msg, uintptr_t, intptr_t, uintptr_t, WindowData *);
static intptr_t CALLBACK log_proc          (HWND,   uint32_t msg, uintptr_t, intptr_t, uintptr_t, WindowData *);
static void              on_create         (HWND);
static void              on_escape_command (char *, WindowData *, HWND);
static int               on_link           (ENLINK *);
static void              on_size           (HWND,   int width,    int height);
static void              on_tab            (HWND,   WindowData *);
static void              put_text          (HWND,   const char *, COLORREF, uint32_t effects);
static void              right_click_menu  (HWND,   intptr_t cursor_pos);
static void              _init             (void);

static const COLORREF CHAT_COLORS[CHAT_LAST+1] = {
	[CHAT_EX]        = RGB(225, 152, 163),
	[CHAT_INGAME]    = RGB(90,  122, 150),
	[CHAT_SELF]      = RGB(50,  170, 230),
	[CHAT_SELFEX]    = RGB(225, 152, 163),
	[CHAT_SYSTEM]    = RGB(80,  150, 80),
	[CHAT_TOPIC]     = RGB(150, 130, 80),
	[CHAT_SERVERIN]  = RGB(153, 190, 215),
	[CHAT_SERVEROUT] = RGB(215, 190, 153),
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

void
ChatBox_set_say_function(HWND window, SayFunction on_say,
    OnCloseFunction on_close, const char *dst)
{
	WindowData *data;

	data = calloc(1, sizeof *data);
	data->dst = dst;
	data->on_say = on_say;
	data->on_close = on_close;
	data->buf_end  = data->buf;

	SetWindowLongPtr(window, GWLP_USERDATA, (intptr_t)data);

	SetWindowSubclass(GetDlgItem(window, DLG_LOG),
	    (SUBCLASSPROC)log_proc, 0, (uintptr_t)data);

	SetWindowSubclass(GetDlgItem(window, DLG_INPUT),
	    (SUBCLASSPROC)input_box_proc, 0, (uintptr_t)data);
}

void
ChatBox_append(HWND window, const char *username, ChatType type, const char *text)
{
	HWND log_window;
	WindowData *data;
	COLORREF color;
	static const CHARRANGE end_of_log = {INT32_MAX, INT32_MAX};

	log_window = GetDlgItem(window, DLG_LOG);
	SendMessage(log_window, EM_EXSETSEL, 0, (intptr_t)&end_of_log);

	if ((type != CHAT_TOPIC) && g_settings.flags & SETTING_TIMESTAMP) {
		char buf[128];
		char *s = buf;

		*(s++) = '[';
		s += GetTimeFormatA(0, 0, NULL, NULL, s, sizeof buf - 2) - 1;
		*(s++) = ']';
		*(s++) = ' ';
		*s = '\0';
		put_text(log_window, buf, COLOR_TIMESTAMP, 0);
	}

	color = CHAT_COLORS[type];
	if (type == CHAT_SERVERIN || type == CHAT_SERVEROUT)
		put_text(log_window, type == CHAT_SERVERIN ? "> " : "< ", color, 0);

	if (username) {
		const User *u;
		const char *alias;

		put_text(log_window, username, color, !(type & CHAT_SYSTEM) * CFE_BOLD);

		u = Users_find(username);
		alias = u ? u->alias : "logged out";
		if (strcmp(alias, UNTAGGED_NAME(username))) {
			char buf[128];

			sprintf(buf, " (%s)", alias);
			put_text(log_window, buf, ALIAS(color), 0);
		}
		put_text(log_window, type & (CHAT_EX | CHAT_SYSTEM) ? " " : ": ",
		    color, type & CHAT_SYSTEM ? 0 : CFE_BOLD);
	}

	put_text(log_window, text, color, 0);
	put_text(log_window, "\n", color, 0);

	data = (WindowData *)GetWindowLongPtr(window, GWLP_USERDATA);
	if (GetTickCount() - data->last_scroll_time > 5000) {
		SendMessage(log_window, WM_VSCROLL, SB_BOTTOM, 0);
		/* sending VM_VSCROLL will reset last_scroll_time in logproc */
		data->last_scroll_time = 0;
	}
}

static void
put_text(HWND window, const char *text, COLORREF color, uint32_t effects)
{
	CHARFORMAT format;
	SETTEXTEX set_text_info;

	format.cbSize = sizeof format;
	format.dwMask = CFM_BOLD | CFM_COLOR;
	format.dwEffects = effects;
	format.crTextColor = color;
	SendMessage(window, EM_SETCHARFORMAT, SCF_SELECTION, (intptr_t)&format);

	set_text_info.flags = ST_SELECTION;
	set_text_info.codepage = 65001;
	SendMessage(window, EM_SETTEXTEX, (uintptr_t)&set_text_info, (intptr_t)text);
}

static void
copy_selected_to_clipboard(HWND window)
{
	CHARRANGE char_range;
	HGLOBAL  *global_mem;
	wchar_t  *selected_text;
	size_t    text_len;

	if (!OpenClipboard(window))
		return;
	SendMessage(window, EM_EXGETSEL, 0, (intptr_t)&char_range);
	text_len = (size_t)(char_range.cpMax - char_range.cpMin + 1);
	global_mem = GlobalAlloc(GMEM_MOVEABLE,
	    sizeof *selected_text * text_len);
	if (!global_mem)
		goto cleanup;

	selected_text = GlobalLock(global_mem);
	SendMessage(window, EM_GETSELTEXT, 0, (intptr_t)selected_text);
	GlobalUnlock(global_mem);
	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, global_mem);
cleanup:
	CloseClipboard();
}

static void
right_click_menu(HWND window, intptr_t cursor_pos)
{
	enum MenuOption {
		COPY, SELECT_ALL, CLEAR, LEAVE
	};

	WindowData      *data;
	HWND             log_window;
	POINT            point;
	HMENU            menu;
	enum MenuOption  menu_option;

	log_window = GetDlgItem(window, DLG_LOG);
	data = (WindowData *)GetWindowLongPtr(window, GWLP_USERDATA);
	menu = CreatePopupMenu();
	AppendMenu(menu, 0, COPY,       L"&Copy");
	AppendMenu(menu, 0, SELECT_ALL, L"Select &All");
	AppendMenu(menu, 0, CLEAR,      L"Clear Log");
	if (data->on_close)
		AppendMenu(menu, 0, LEAVE, L"Leave");

	point.x = LOWORD(cursor_pos);
	point.y = HIWORD(cursor_pos);
	ClientToScreen(log_window, &point);

	menu_option = TrackPopupMenuEx(menu, TPM_RETURNCMD, point.x, point.y,
	    window, NULL);
	DestroyMenu(menu);

	switch (menu_option) {
	case COPY:
		copy_selected_to_clipboard(log_window);
		return;

	case SELECT_ALL: {
		CHARRANGE char_range;

		char_range.cpMin = 0;
		char_range.cpMax = -1;
		SendMessage(log_window, EM_EXSETSEL, 0, (intptr_t)&char_range);
		return;
	}
	case CLEAR:
		SetWindowText(log_window, NULL);
		return;

	case LEAVE:
		data->on_close(data->dst);
		return;
	}
}

static intptr_t CALLBACK
chat_box_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
    switch (msg) {
	case WM_CREATE:
		on_create(window);
		return 0;

	case WM_CLOSE: {
		WindowData *data;

		data = (WindowData *)GetWindowLongPtr(window, GWLP_USERDATA);
		if (data->on_close)
			data->on_close(data->dst);
		free(data);
		return 0;
	}
	case WM_SIZE:
		on_size(window, LOWORD(l_param), HIWORD(l_param));
		return 0;

	case WM_COMMAND:
		if (w_param == MAKEWPARAM(DLG_LOG, EN_VSCROLL)) {
			WindowData *data;

			data = (WindowData *)GetWindowLongPtr(window, GWLP_USERDATA);
			data->last_scroll_time = GetTickCount();
			return 0;
		}
		break;

	case WM_NOTIFY: {
		NMHDR *note = (NMHDR *)l_param;

		if (note->idFrom != DLG_LOG)
			break;

		if (note->code == EN_LINK)
			return on_link((ENLINK *)l_param);

		if (note->code == EN_MSGFILTER) {
			MSGFILTER *info = (void *)l_param;
			if (info->msg != WM_RBUTTONUP)
				return 0;

			right_click_menu(window, info->lParam);
			return 1;
		}
		break;
	}
	}
	return DefWindowProc(window, msg, w_param, l_param);
}

static int
on_link(ENLINK *link_info)
{
	wchar_t   url[256];
	GETTEXTEX get_text_info;

	if (link_info->msg != WM_LBUTTONUP)
		return 0;

	SendMessage(link_info->nmhdr.hwndFrom, EM_EXSETSEL, 0,
	    (intptr_t)&link_info->chrg);

	get_text_info.cb       = sizeof url;
	get_text_info.flags    = GT_SELECTION;
	get_text_info.codepage = 1200;

	SendMessage(link_info->nmhdr.hwndFrom, EM_GETTEXTEX,
	    (uintptr_t)&get_text_info, (intptr_t)url);

	ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
	return 1;
}

static void
on_escape_command(char *command, WindowData *data, HWND window)
{
	char *code;

	code = strsep(&command, " ");

	if (!strcmp(code, "me")) {
		data->on_say(command, true, data->dst);
		return;
	}

	if (!strcmp(code, "resync")) {
		Sync_reload();
		return;
	}

	if (!strcmp(code, "dlmap")) {
		/* DownloadMap(command); */
		return;
	}

	if (!strcmp(code, "start")) {
		Spring_launch();
		return;
	}

	if (!strcmp(code, "msg") || !strcmp(code, "pm")) {
		char  buf[128];
		char *username;
		User *user;

		username = strsep(&command, " ");
		if ((user = Users_find(username))) {
			TasServer_send_say_private(command, false, user->name);
			return;
		}
		sprintf(buf, "Could not send message: %s is not logged in.", username);
		ChatBox_append(GetParent(window), NULL, CHAT_SYSTEM, buf);
		return;
	}

	if (!strcmp(code, "j") || !strcmp(code, "join")) {
		TasServer_send_join_channel(command + (*command == '#'));
		return;
	}

	if (!strcmp(code, "away")) {
		ClientStatus cs;

		cs = g_last_client_status;
		cs.away = !cs.away;
		TasServer_send_my_client_status(cs);
		return;
	}

	if (!strcmp(code, "ingametime")) {
		TasServer_send_get_ingame_time();
		return;
	}

	if (!strcmp(code, "split")) {
		char      *split_type;
		SplitType  type;

		split_type = strsep(&command, " ");
		type = !strcmp(split_type, "h") ? SPLIT_HORZ
			: !strcmp(split_type, "v") ? SPLIT_VERT
			: !strcmp(split_type, "c1") ? SPLIT_CORNERS1
			: !strcmp(split_type, "c2") ? SPLIT_CORNERS2
			: SPLIT_LAST+1;
		if (type <= SPLIT_LAST)
			MyBattle_set_split(type, atoi(command));
		return;
	}

	if (!strcmp(code, "ingame")) {
		ClientStatus cs;

		cs = g_last_client_status;
		cs.ingame = !cs.ingame;
		TasServer_send_my_client_status(cs);
		return;
	}
}

static void
on_tab(HWND window, WindowData *data)
{
	char text[MAX_TEXT_MESSAGE_LENGTH];
	int  offset;
	int  end;
	int  start;

	GetWindowTextA(window, text, LENGTH(text));
	if (data->last_pos != SendMessage(window, EM_GETSEL, 0, 0) || data->end < 0) {
		data->end = LOWORD(SendMessage(window, EM_GETSEL, 0, 0));
		data->offset = 0;
		data->last_index = 0;
	}
	offset = data->offset;
	end = data->end + offset;
	start = data->end + offset;

	while (start - offset && !isspace(text[start - 1 - offset]))
		--start;

	text[end] = L'\0';
}

static intptr_t CALLBACK
input_box_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param,
		__attribute__((unused)) uintptr_t _unused, WindowData *data)
{
	if (msg != WM_KEYDOWN)
		return DefSubclassProc(window, msg, w_param, l_param);

	if (w_param != VK_TAB)
		data->end = -1;

	switch (w_param) {
	case VK_TAB:
		on_tab(window, data);
		return 0;

	case VK_DOWN:
		if (!data->input_hint)
			return 0;

		while (++data->input_hint != data->buf
		    && data->input_hint != data->buf_end
		    && ((data->input_hint)[-1] || !*data->input_hint))
			if (data->input_hint >= data->buf+LENGTH(data->buf)-1)
				data->input_hint = data->buf - 1;

		SetWindowText(window, data->input_hint);
		return 0;

	case VK_UP: {
		if (!data->input_hint)
			return 0;
		while ((--data->input_hint != data->buf)
				&& data->input_hint != data->buf_end
				&& ((data->input_hint)[-1] || !*data->input_hint))
			if (data->input_hint < data->buf)
				data->input_hint = data->buf + LENGTH(data->buf) - 1;
		SetWindowText(window, data->input_hint);
	}	return 0;

	case '\r': {
		int  len;
		char *text_a;

		if ((ptrdiff_t)LENGTH(data->buf) - (data->buf_end - data->buf)
		    < MAX_TEXT_MESSAGE_LENGTH) {
			size_t len2;
			data->buf_end = data->buf;
			len2 = (LENGTH(data->buf) - (size_t)(data->buf_end - data->buf)) * sizeof *data->buf;
			memset(data->buf_end, '\0', len2);
		}
		len = GetWindowTextW(window, data->buf_end,
		    MAX_TEXT_MESSAGE_LENGTH);
		if (len <= 0)
			return 0;
		text_a = _alloca((size_t)len * 3);
		WideCharToMultiByte(CP_UTF8, 0, data->buf_end, -1, text_a,
		    (int)len * 3, NULL, NULL);

		SetWindowText(window, NULL);

		if (text_a[0] == '/') {
			on_escape_command(text_a + 1, data, window);
		} else {
			data->on_say(text_a, false, data->dst);
		}

		data->last_scroll_time = 0;
		data->buf_end += len + 1;
		data->input_hint = data->buf_end;
	}	return 0;

	default:
		return DefSubclassProc(window, msg, w_param, l_param);
	}
}

static intptr_t CALLBACK
log_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param,
		__attribute__((unused)) uintptr_t uIdSubclass, WindowData *data)
{
	if (msg == WM_VSCROLL)
		data->last_scroll_time = GetTickCount();

	return DefSubclassProc(window, msg, w_param, l_param);
}

static void
on_size(HWND window, int width, int height)
{
	HWND input;
	HWND log;

	log = GetDlgItem(window, DLG_LOG);
	MoveWindow(log, 0, 0, width, height - MAP_Y(14), 1);

	input = GetDlgItem(window, DLG_INPUT);
	MoveWindow(input, 0, height - MAP_Y(14), width, MAP_Y(14), 1);
}

static void
on_create(HWND window)
{
	HWND log_window;

	CreateDlgItems(window, DIALOG_ITEMS, DLG_LAST + 1);
	log_window = GetDlgItem(window, DLG_LOG);
	SendMessage(log_window, EM_EXLIMITTEXT, 0, INT_MAX);
	SendMessage(log_window, EM_AUTOURLDETECT, TRUE, 0);
	SendMessage(log_window, EM_SETEVENTMASK, 0, ENM_LINK | ENM_MOUSEEVENTS
	    | ENM_SCROLL);
}

static void __attribute__((constructor))
_init(void)
{
	WNDCLASS window_class = {
		.lpszClassName = WC_CHATBOX,
		.lpfnWndProc   = (WNDPROC)chat_box_proc,
		.hCursor       = LoadCursor(NULL, (wchar_t *)IDC_ARROW),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
	};

	RegisterClass(&window_class);
}
