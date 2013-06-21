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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <commctrl.h>

#include "wincommon.h"
#include "chatlist.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

enum {
	DLG_LIST,
};

static const DialogItem DIALOG_ITEMS[] = {
	[DLG_LIST] = {
		.class = WC_LISTVIEW,
		.ex_style = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | LVS_SHAREIMAGELISTS | LVS_SINGLESEL | LVS_REPORT | LVS_SORTASCENDING | LVS_NOCOLUMNHEADER,
	},
};

static void
on_create(HWND window)
{
	CreateDlgItems(window, DIALOG_ITEMS, LENGTH(DIALOG_ITEMS));
	puts("here");

}

static void
on_size(HWND window, uint16_t width, uint16_t height)
{
	MoveWindow(GetDlgItem(window, DLG_LIST), 0, 0, width, height, 1);
}

static LRESULT CALLBACK
chatlist_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg) {

	case WM_CREATE:
		on_create(window);
		return 0;

	case WM_SIZE:
		on_size(window, LOWORD(l_param), HIWORD(l_param));
		return 0;

	/* case WM_COMMAND: */
		/* return on_command(w_param); */

	/* case WM_HSCROLL: */
		/* if (GetDlgCtrlID((HWND)l_param) == DLG_SPLIT_SIZE */
				/* && w_param == SB_ENDSCROLL) */
			/* on_split_size_scroll(); */
		/* return 0; */

	default:
		return DefWindowProc(window, msg, w_param, l_param);
	}
}

static void __attribute__((constructor))
_init (void)
{
	WNDCLASSEX window_class = {
		.lpszClassName = WC_CHATLIST,
		.cbSize        = sizeof(WNDCLASSEX),
		.lpfnWndProc   = chatlist_proc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};

	RegisterClassEx(&window_class);
}
#if 0
		HWND list = CreateDlgItem(window, &DIALOG_ITEMS[DLG_LIST2], DLG_LIST2);

		for (int i=0; i<=COLUMN_LAST; ++i) {
			LVCOLUMN info;
			info.mask = 0;
			SendMessage(list, LVM_INSERTCOLUMN, 0, (LPARAM)&info);
		}
		ListView_SetExtendedListViewStyle(list, LVS_EX_DOUBLEBUFFER | LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
		IconList_enable_for_listview(list);
		if (note->idFrom == DLG_LIST) {
			if (note->code == LVN_ITEMACTIVATE) {
				LVITEM item;
				item.mask = LVIF_PARAM;
				item.iItem = ((LPNMITEMACTIVATE)l_param)->iItem;

				if (item.iItem < 0)
					return 0;

				SendMessage(note->hwndFrom, LVM_GETITEM, 0, (LPARAM)&item);
				ChatWindow_set_active_tab(Chat_get_private_window((User *)item.lParam));
				return 0;
			}

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
			if (note->code == LVN_ITEMACTIVATE) {
				ChatWindow_set_active_tab(Chat_get_private_window(u));
				return 0;
			}
			HMENU menu = CreatePopupMenu();
			AppendMenu(menu, 0, 1, L"Private chat");
			AppendMenu(menu, u->ignore * MF_CHECKED, 2, L"Ignore");
			AppendMenu(menu, 0, 3, L"Set alias");
			SetMenuDefaultItem(menu, 1, 0);
			ClientToScreen(note->hwndFrom, &((LPNMITEMACTIVATE)l_param)->ptAction);
			int clicked = TrackPopupMenuEx(menu, TPM_RETURNCMD, ((LPNMITEMACTIVATE)l_param)->ptAction.x, ((LPNMITEMACTIVATE)l_param)->ptAction.y, window, NULL);
			DestroyMenu(menu);
			if (clicked == 1)
				ChatWindow_set_active_tab(Chat_get_private_window(u));
			else if (clicked == 2) {
				u->ignore = !u->ignore;
				/* UpdateUser(u); */
			} else if (clicked == 3) {
				char title[128], buf[MAX_NAME_LENGTH_NUL];
				sprintf(title, "Set alias for %s", u->name);
				if (!GetTextDialog_create(title, strcpy(buf, UNTAGGED_NAME(u->name)), MAX_NAME_LENGTH_NUL)) {
					strcpy(u->alias, buf);
				}
			}
			return 0;

#endif

#if 0
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
#endif

#if 0
/* on tab stuff */
	/* HWND list = GetDlgItem(GetParent(type ? window : GetParent(window)), DLG_LIST); */

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
#endif

#if 0

void
Chat_on_disconnect(void)
{
	/* SendDlgItemMessage(g_server_chat_window, DLG_LIST, LVM_DELETEALLITEMS, 0, 0); */
	/* for (size_t i=0; i<LENGTH(g_channel_windows); ++i) { */
		/* if (!g_channel_windows[i]) */
			/* continue; */
		/* SendDlgItemMessage(g_channel_windows[i], DLG_LIST, */
				/* LVM_DELETEALLITEMS, 0, 0); */
	/* } */
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
#endif
