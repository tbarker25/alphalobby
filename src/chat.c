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
#include "chat.h"
#include "chat_window.h"
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
	COLUMN_NAME,
	COLUMN_COUNTRY,
	COLUMN_LAST = COLUMN_COUNTRY,
};

enum {
	DLG_LOG,
	DLG_INPUT,
	DLG_LIST,
	DLG_LAST = DLG_LIST,
};

#define ALIAS(c)\
	RGB(128 + GetRValue(c) / 2, 128 + GetGValue(c) / 2, 128 + GetBValue(c) / 2)
#define COLOR_TIMESTAMP RGB(200,200,200)

typedef struct ChatWindowData {
	char *name;
	ChatDest type;
}ChatWindowData;

typedef struct InputBoxData {
	int last_index, end, last_pos, offset;
	wchar_t buf[8192], *input_hint, *buffTail;
}InputBoxData;

static void             _init             (void);
static void             put_text          (HWND window, const char *text, COLORREF color, DWORD effects);
static LRESULT CALLBACK chat_box_proc     (HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
static void             on_create         (HWND window, LPARAM l_param);
static void             on_size           (HWND window, int width, int height);
static LRESULT CALLBACK log_proc          (HWND window, UINT msg, WPARAM w_param, LPARAM l_param, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
static LRESULT CALLBACK input_box_proc    (HWND window, UINT msg, WPARAM w_param, LPARAM l_param, UINT_PTR type, InputBoxData *data);
static void             on_escape_command (char *s, UINT_PTR type, const wchar_t *dest_name, HWND window);
static void             on_tab            (HWND window, UINT_PTR type, InputBoxData *data);
static BOOL CALLBACK    enum_child_proc   (HWND window, LPARAM user);
static void             update_user       (HWND window, User *u, int index);

static HWND g_channel_windows[128];
static HWND g_server_chat_window;

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

static DialogItem DIALOG_ITEMS[] = {
	[DLG_LOG] = {
		.class = RICHEDIT_CLASS,
		.ex_style = WS_EX_WINDOWEDGE,
		.style = WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
	}, [DLG_INPUT] = {
		.class = WC_EDIT,
		.ex_style = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | ES_AUTOHSCROLL,
	}, [DLG_LIST] = {
		.class = WC_LISTVIEW,
		.ex_style = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | LVS_SHAREIMAGELISTS | LVS_SINGLESEL | LVS_REPORT | LVS_SORTASCENDING | LVS_NOCOLUMNHEADER,
	}
};

void
Chat_on_left_battle(HWND window, User *u)
{
	window = GetDlgItem(window, DLG_LIST);
	int i = SendMessage(window, LVM_FINDITEM, -1,
			(LPARAM)&(LVFINDINFO){.flags = LVFI_PARAM, .lParam = (LPARAM)u});
	SendMessage(window, LVM_DELETEITEM, i, 0);
}

static void
update_user(HWND window, User *u, int index)
{
	char *name;
	if (!strcmp(UNTAGGED_NAME(u->name), u->alias))
		name = u->name;
	else {
		name = _alloca(MAX_NAME_LENGTH * 2 + sizeof(" ()"));
		sprintf(name, "%s (%s)", u->name, u->alias);
	}

	LVITEMA item;
	item.mask = LVIF_IMAGE | LVIF_STATE | LVIF_TEXT;
	item.iItem = index;
	item.iSubItem = 0;
	item.pszText = name;
	item.iImage = u->ingame ? ICON_INGAME
		    : u->battle ? ICONMASK_INGAME
		    : -1;
	item.stateMask = LVIS_OVERLAYMASK;

	int icon_index = ICONMASK_USER;
	if (u->away)
		icon_index |= ICONMASK_AWAY;
	if (u->ignore)
		icon_index |= ICONMASK_IGNORE;

	item.state = INDEXTOOVERLAYMASK(icon_index);

	SendMessage(window, LVM_SETITEMA, 0, (LPARAM)&item);
}

static BOOL CALLBACK
enum_child_proc(HWND window, LPARAM user)
{
	HWND list = GetDlgItem(window, DLG_LIST);
	if (!list)
		return 1;

	LVFINDINFO find_info;
	find_info.flags = LVFI_PARAM;
	find_info.lParam = user;

	int index = SendMessage(list, LVM_FINDITEM, -1,
			(LPARAM)&find_info);

	if (index >= 0)
		update_user(list, (User *)user, index);

	return 1;
}

void
Chat_update_user(User *u)
{
	EnumChildWindows(g_tab_control, enum_child_proc, (LPARAM)u);
}

void
Chat_add_user(HWND window, User *u)
{
	HWND list = GetDlgItem(window, DLG_LIST);
	LVITEMA item;
	item.mask = LVIF_PARAM | LVIF_TEXT;
	item.iSubItem = 0;
	item.pszText = u->name;
	item.lParam = (LPARAM)u;

	int index = SendMessage(list, LVM_INSERTITEMA, 0, (LPARAM)&item);
	update_user(list, u, index);

	SendMessage(list, LVM_SETITEM, 0, (LPARAM)&(LVITEM){
		.mask = LVIF_IMAGE, .iImage = ICON_FIRST_FLAG + u->country,
		.iItem = index, .iSubItem = COLUMN_COUNTRY,
	});

	RECT rect;
	GetClientRect(list, &rect);
	SendMessage(list, LVM_SETCOLUMNWIDTH, 0, rect.right - ICON_SIZE);
	SendMessage(list, LVM_SETCOLUMNWIDTH, 1, ICON_SIZE);
}

static void
on_tab(HWND window, UINT_PTR type, InputBoxData *data)
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

	HWND list = GetDlgItem(GetParent(type ? window : GetParent(window)), DLG_LIST);

	if (!list)
		return;

	int count = ListView_GetItemCount(list);
	if (!count)
		return;

	data->last_index %= count;
	LVITEM info;
	info.mask = LVIF_PARAM;
	info.iItem = data->last_index;
	do {
		SendMessage(list, LVM_GETITEM, 0, (LPARAM)&info);
		const char *name = ((User *)info.lParam)->name;
		const char *s = NULL;
		if (!_strnicmp(name, text+start, strlen(text+start))
		    || ((s = strchr(name, ']')) && !_strnicmp(s + 1, text+start, strlen(text+start)))) {
			data->last_index = info.iItem + 1;
			SendMessage(window, EM_SETSEL, start - offset, LOWORD(SendMessage(window, EM_GETSEL, 0, 0)));
			data->offset = s ? s - name + 1 : 0;
			SendMessageA(window, EM_REPLACESEL, 1, (LPARAM)name);
			break;
		}
		info.iItem = (info.iItem + 1) % count;
	} while (info.iItem != data->last_index);

	data->last_pos = SendMessage(window, EM_GETSEL, 0, 0);
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

	} else if (!strcmp(code, "resync"))
		Sync_reload();
	else if (!strcmp(code, "dlmap"))
		DownloadMap(s);
	else if (!strcmp(code, "start"))
		Spring_launch();
	else if (!strcmp(code, "msg") || !strcmp(code, "pm")) {
		char *username = strsep(&s, " ");
		User *u = Users_find(username);
		if (u) {
			TasServer_send_say_private(u, s);
			ChatWindow_set_active_tab(Chat_get_private_window(u));
		} else {
			char buf[128];
			sprintf(buf, "Could not send message: %s is not logged in.", username);
			Chat_said(GetParent(window), NULL, CHAT_SYSTEM, buf);
		}
	} else if (!strcmp(code, "j") || !strcmp(code, "join"))
		Chat_join_channel(s, 1);
	else if (!strcmp(code, "away")) {
		ClientStatus cs = g_last_client_status;
		cs.away = !cs.away;
		TasServer_send_my_client_status(cs);
	} else if (!strcmp(code, "ingametime"))
		TasServer_send_get_ingame_time();
	else if (!strcmp(code, "split")) {
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
		UINT_PTR type, InputBoxData *data)
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

		} else if (type == DEST_BATTLE) {
			TasServer_send_say_battle(text_a);

		} else if (type == DEST_PRIVATE) {
			TasServer_send_say_private((User *)utf16to8(dest_name), text_a);

		} else if (type == DEST_CHANNEL) {
			TasServer_send_say_channel(utf16to8(dest_name), text_a);

		} else if (type == DEST_SERVER) {
			/* TODO */
			/* TasServer_send("%s", text_a); */

		} else {
			assert(0);
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
	HWND list = GetDlgItem(window, DLG_LIST);
	HWND input = GetDlgItem(window, DLG_INPUT);
	HWND log = GetDlgItem(window, DLG_LOG);
	MoveWindow(log, 0, 0, width - (list ? MAP_X(MARGIN) : 0),
			height - !!input * MAP_Y(14), 1);
	MoveWindow(input, 0, height - MAP_Y(14),
			width - (list ? MAP_X(MARGIN): 0), MAP_Y(14), 1);
	BringWindowToTop(list);
	MoveWindow(list, width - MAP_X(MARGIN), 0, MAP_X(MARGIN), height, 1);
	RECT rect;
	GetClientRect(list, &rect);
	SendMessage(list, LVM_SETCOLUMNWIDTH, 0, rect.right - ICON_SIZE);
	SendMessage(list, LVM_SETCOLUMNWIDTH, 1, ICON_SIZE);
}

static void
on_create(HWND window, LPARAM l_param)
{
	static ChatWindowData battle_room_data = {NULL, DEST_BATTLE};
	ChatWindowData *data = ((CREATESTRUCT *)l_param)->lpCreateParams ?: &battle_room_data;
	SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)data);
	if (data->name)
		SetWindowTextA(window, data->name);
	HWND log_window = CreateDlgItem(window, &DIALOG_ITEMS[DLG_LOG], DLG_LOG);
	SendMessage(log_window, EM_EXLIMITTEXT, 0, INT_MAX);
	SendMessage(log_window, EM_AUTOURLDETECT, TRUE, 0);
	SendMessage(log_window, EM_SETEVENTMASK, 0, ENM_LINK | ENM_MOUSEEVENTS | ENM_SCROLL);
	SetWindowSubclass(log_window, log_proc, 0, 0);

	HWND input_box = CreateDlgItem(window, &DIALOG_ITEMS[DLG_INPUT], DLG_INPUT);
	InputBoxData *input_box_data = calloc(1, sizeof(*input_box_data));
	input_box_data->buffTail = input_box_data->buf;

	SetWindowSubclass(input_box, (void *)input_box_proc, (UINT_PTR)data->type, (DWORD_PTR)input_box_data);

	if (data->type >= DEST_CHANNEL) {
		HWND list = CreateDlgItem(window, &DIALOG_ITEMS[DLG_LIST], DLG_LIST);
		for (int i=0; i<=COLUMN_LAST; ++i) {
			LVCOLUMN info;
			info.mask = 0;
			SendMessage(list, LVM_INSERTCOLUMN, 0, (LPARAM)&info);
		}
		ListView_SetExtendedListViewStyle(list, LVS_EX_DOUBLEBUFFER | LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
		IconList_enable_for_listview(list);
	}
}

static LRESULT CALLBACK
chat_box_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
    switch (msg) {
	case WM_CREATE:
		on_create(window, l_param);
		break;
	case WM_CLOSE: {
		ChatWindowData *data = (void *)GetWindowLongPtr(window, GWLP_USERDATA);
		if (data->type == DEST_CHANNEL)
			TasServer_send_leave_channel(data->name);
		ChatWindow_remove_tab(window);
	}	return 0;
	case WM_SIZE:
		on_size(window, LOWORD(l_param), HIWORD(l_param));
		return 1;
	case WM_COMMAND:
		if (w_param == MAKEWPARAM(DLG_LOG, EN_VSCROLL))
			SetWindowLongPtr((HWND)l_param, GWLP_USERDATA, GetTickCount());
		return 0;
	case WM_NOTIFY: {
		NMHDR *note = (NMHDR *)l_param;
		if (note->idFrom == DLG_LIST) {
			LVITEM item = {
					.mask = LVIF_PARAM,
					.iItem = -1,
				};
			if (note->code == NM_RCLICK)
				item.iItem = SendMessage(note->hwndFrom, LVM_SUBITEMHITTEST, 0, (LPARAM)&(LVHITTESTINFO){.pt = ((LPNMITEMACTIVATE)l_param)->ptAction});
			else if (note->code == LVN_ITEMACTIVATE)
				item.iItem = ((LPNMITEMACTIVATE)l_param)->iItem;

			if (item.iItem < 0)
				break;

			SendMessage(note->hwndFrom, LVM_GETITEM, 0, (LPARAM)&item);
			User *u = (void *)item.lParam;
			if (u == &g_my_user)
				return 0;
			if (note->code == LVN_ITEMACTIVATE)
				goto addtab;
			HMENU menu = CreatePopupMenu();
			AppendMenu(menu, 0, 1, L"Private chat");
			AppendMenu(menu, u->ignore * MF_CHECKED, 2, L"Ignore");
			AppendMenu(menu, 0, 3, L"Set alias");
			SetMenuDefaultItem(menu, 1, 0);
			ClientToScreen(note->hwndFrom, &((LPNMITEMACTIVATE)l_param)->ptAction);
			int clicked = TrackPopupMenuEx(menu, TPM_RETURNCMD, ((LPNMITEMACTIVATE)l_param)->ptAction.x, ((LPNMITEMACTIVATE)l_param)->ptAction.y, window, NULL);
			DestroyMenu(menu);
			if (clicked == 1)
				addtab:
				ChatWindow_set_active_tab(Chat_get_private_window(u));
			else if (clicked == 2) {
				u->ignore ^= 1;
				/* UpdateUser(u); */
			} else if (clicked == 3) {
				char title[128], buf[MAX_NAME_LENGTH_NUL];
				sprintf(title, "Set alias for %s", u->name);
				if (!GetTextDialog_create(title, strcpy(buf, UNTAGGED_NAME(u->name)), MAX_NAME_LENGTH_NUL)) {
					strcpy(u->alias, buf);
				}
			}
			return 0;

		} else if (note->idFrom == DLG_LOG && note->code == EN_MSGFILTER) {
			MSGFILTER *s = (void *)l_param;
			if (s->msg != WM_RBUTTONUP)
				break;

			ChatWindowData *data = (void *)GetWindowLongPtr(window, GWLP_USERDATA);

			HMENU menu = CreatePopupMenu();
			AppendMenu(menu, 0, 1, L"Copy");
			AppendMenu(menu, 0, 2, L"Clear window");
			AppendMenu(menu, g_settings.flags & SETTING_TIMESTAMP ? MF_CHECKED : 0, 3, L"Timestamp messages");

			if (data->type == DEST_PRIVATE)
				AppendMenu(menu, 0, 6, L"Ignore user");

			AppendMenu(menu, g_settings.flags & (1<<data->type) ? MF_CHECKED : 0, 4, L"Show login/logout");

			AppendMenu(menu, 0, 5, (const wchar_t *[]){L"Leave Battle", L"Close private chat", L"Leave channel", L"Hide server log"}[data->type]);

			POINT pt = {.x = LOWORD(s->lParam), .y = HIWORD(s->lParam)};
			ClientToScreen(s->nmhdr.hwndFrom, &pt);

			int index = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y, window, NULL);
			switch (index) {
			case 1: {
				CHARRANGE s;
				SendMessage(note->hwndFrom, EM_EXGETSEL, 0, (LPARAM)&s);
				HGLOBAL *mem_buff = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * (s.cpMax - s.cpMin + 1));
				SendMessage(note->hwndFrom, EM_GETSELTEXT, 0, (LPARAM)GlobalLock(mem_buff));
				GlobalUnlock(mem_buff);
				OpenClipboard(window);
				EmptyClipboard();
				SetClipboardData(CF_UNICODETEXT, mem_buff);
				CloseClipboard();
				break;
			}
			case 2:
				SetWindowText(s->nmhdr.hwndFrom, NULL);
				break;
			case 3: case 4: {
				MENUITEMINFO info = {.cbSize = sizeof(MENUITEMINFO), .fMask = MIIM_STATE};
				GetMenuItemInfo(menu, index, 0, &info);
				int new_val = !(info.fState & MFS_CHECKED);
				if (index == 3)
					data->type = DEST_LAST + 1;
				g_settings.flags = (g_settings.flags & ~(1<<data->type)) | new_val<<data->type;
			}	break;
			case 6: {
				User *u = Users_find(data->name);
				if (u)
					u->ignore = 1;
			}	// FALLTHROUGH:
			case 5:
				SendMessage(data->type == DEST_BATTLE ? GetParent(window) : window, WM_CLOSE, 0, 0);
				break;
			}

		} else if (note->idFrom == DLG_LOG && note->code == EN_LINK) {
			ENLINK *s = (void *)l_param;
			if (s->msg != WM_LBUTTONUP)
				break;
			SendMessage(note->hwndFrom, EM_EXSETSEL, 0, (LPARAM)&s->chrg);
			wchar_t url[256];
			SendMessage(note->hwndFrom, EM_GETTEXTEX,
					(WPARAM)&(GETTEXTEX){.cb = sizeof(url), .flags = GT_SELECTION, .codepage = 1200},
					(LPARAM)url);
			ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
			return 0;
		}
	}	break;
	}
	return DefWindowProc(window, msg, w_param, l_param);
}

static void
put_text(HWND window, const char *text, COLORREF color, DWORD effects)
{
	SendMessage(window, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&(CHARFORMAT){
		.cbSize = sizeof(CHARFORMAT),
		.dwMask = CFM_BOLD | CFM_COLOR | CFM_ITALIC | CFM_STRIKEOUT | CFM_UNDERLINE,
		.dwEffects = effects,
		.crTextColor = color,
	});
	SendMessage(window, EM_SETTEXTEX, (WPARAM)&(SETTEXTEX){.flags = ST_SELECTION, .codepage = 65001}, (LPARAM)text);
}

void
Chat_said(HWND window, const char *username, ChatType type, const char *text)
{
	window = GetDlgItem(window, DLG_LOG);
	SendMessage(window, EM_EXSETSEL, 0, (LPARAM)&(CHARRANGE){INFINITE, INFINITE});

	char buf[128];
	if ((type != CHAT_TOPIC) && g_settings.flags & SETTING_TIMESTAMP) {
		char *s = buf;
		*(s++) = '[';
		s += GetTimeFormatA(0, 0, NULL, NULL, s, sizeof(buf) - 2) - 1;
		*(s++) = ']';
		*(s++) = ' ';
		*s = '\0';
		put_text(window, buf, COLOR_TIMESTAMP, 0);
	}

	COLORREF color = CHAT_COLORS[type];
	if (type == CHAT_SERVERIN || type == CHAT_SERVEROUT)
		put_text(window, type == CHAT_SERVERIN ? "> " : "< ", color, 0);

	if (username) {
		put_text(window, username, color, !(type & CHAT_SYSTEM) * CFE_BOLD);
		const User *u = Users_find(username);
		const char *alias = u ? u->alias : "logged out";
		if (strcmp(alias, UNTAGGED_NAME(username))) {
			sprintf(buf, " (%s)", alias);
			put_text(window, buf, ALIAS(color), 0);
		}
		put_text(window, type & (CHAT_EX | CHAT_SYSTEM) ? " " : ": ", color, !(type & CHAT_SYSTEM) * CFE_BOLD);
	}

	put_text(window, text, color, 0);
	put_text(window, "\n", color, 0);

	if (GetTickCount() - GetWindowLongPtr(window, GWLP_USERDATA) > 5000) {
		SendMessage(window, WM_VSCROLL, SB_BOTTOM, 0);
		SetWindowLongPtr(window, GWLP_USERDATA, 0);
	}
}

HWND
Chat_get_channel_window(const char *name)
{
	for (size_t i=0; i<LENGTH(g_channel_windows); ++i) {
		if (!g_channel_windows[i]) {
			ChatWindowData *data = malloc(sizeof(*data));
			*data = (ChatWindowData){_strdup(name), DEST_CHANNEL};
			g_channel_windows[i] = CreateWindow(WC_CHATBOX, NULL, WS_CHILD,
			0, 0, 0, 0,
			g_tab_control, (HMENU)DEST_CHANNEL, NULL, (void *)data);
			return g_channel_windows[i];
		}
		ChatWindowData *data = (void *)GetWindowLongPtr(g_channel_windows[i], GWLP_USERDATA);
		if (!strcmp(name, data->name))
			return g_channel_windows[i];
	}
	return NULL;
}

HWND
Chat_get_private_window(User *u)
{
	if (!u->chat_window) {
		ChatWindowData *data = malloc(sizeof(*data));
		*data = (ChatWindowData){u->name, DEST_PRIVATE};
		u->chat_window = CreateWindow(WC_CHATBOX, NULL, WS_CHILD,
			0, 0, 400, 400,
			g_tab_control, (HMENU)DEST_PRIVATE, NULL, (void *)data);
	}
	return u->chat_window;
}

HWND
Chat_get_server_window(void)
{
	if (!g_server_chat_window) {
		ChatWindowData *data = malloc(sizeof(*data));
		*data = (ChatWindowData){"TAS Server", DEST_SERVER};
		g_server_chat_window = CreateWindow(WC_CHATBOX, NULL, WS_CHILD, 0, 0, 0, 0, g_tab_control, (HMENU)0, NULL, (void *)data);
		ChatWindow_add_tab(g_server_chat_window);
	}
	return g_server_chat_window;
}

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

void
Chat_on_disconnect(void)
{
	SendDlgItemMessage(g_server_chat_window, DLG_LIST, LVM_DELETEALLITEMS, 0, 0);
	for (size_t i=0; i<LENGTH(g_channel_windows); ++i) {
		if (!g_channel_windows[i])
			continue;
		SendDlgItemMessage(g_channel_windows[i], DLG_LIST,
				LVM_DELETEALLITEMS, 0, 0);
	}
}

void
Chat_join_channel(const char *channel_name, int focus)
{
	channel_name += *channel_name == '#';

	for (const char *c=channel_name; *c; ++c) {
		if (!isalnum(*c) && *c != '[' && *c != ']') {
			MainWindow_msg_box("Couldn't join channel", "Unicode channels are not allowed.");
			return;
		}
	}

	TasServer_send_join_channel(channel_name);
	if (focus)
		ChatWindow_set_active_tab(Chat_get_channel_window(channel_name));
	}
