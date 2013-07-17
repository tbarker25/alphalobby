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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <commctrl.h>

#include "common.h"
#include "chatlist.h"
#include "chattab.h"
#include "iconlist.h"
#include "layoutmetrics.h"
#include "user.h"
#include "dialogs/gettextdialog.h"
#include "wincommon.h"

#define LENGTH(x) (sizeof x / sizeof *x)

enum {
	DLG_LIST,
};

enum ColumnId {
	COLUMN_NAME,
	COLUMN_COUNTRY,
	COLUMN_LAST = COLUMN_COUNTRY,
};

static const DialogItem DIALOG_ITEMS[] = {
	[DLG_LIST] = {
		.class = WC_LISTVIEW,
		.ex_style = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | LVS_SHAREIMAGELISTS | LVS_SINGLESEL
			| LVS_REPORT | LVS_SORTASCENDING | LVS_NOCOLUMNHEADER,
	},
};

static void
on_create(HWND window)
{
	HWND list;

	list = CreateDlgItem(window, &DIALOG_ITEMS[DLG_LIST], DLG_LIST);
	for (int i=0; i<=COLUMN_LAST; ++i) {
		LVCOLUMN info;
		info.mask = 0;
		SendMessage(list, LVM_INSERTCOLUMN, 0, (intptr_t)&info);
	}
	ListView_SetExtendedListViewStyle(list,
	    LVS_EX_DOUBLEBUFFER | LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
	IconList_enable_for_listview(list);
}

static void
on_size(HWND window, uint16_t width, uint16_t height)
{
	RECT rect;
	HWND list;

	list = GetDlgItem(window, DLG_LIST);
	MoveWindow(list, 0, 0, width, height, 1);
	GetClientRect(list, &rect);
	SendMessage(list, LVM_SETCOLUMNWIDTH, 0, rect.right - ICON_SIZE);
	SendMessage(list, LVM_SETCOLUMNWIDTH, 1, ICON_SIZE);
}

static void
on_activate(NMITEMACTIVATE *note)
{
	LVITEM item;

	if (note->iItem < 0)
		return;
	item.mask = LVIF_PARAM;
	item.iItem = note->iItem;
	SendMessage(note->hdr.hwndFrom, LVM_GETITEM, 0, (intptr_t)&item);
	ChatTab_focus_private((User *)item.lParam);
}

static void
on_right_click(NMITEMACTIVATE *note)
{
	enum MenuOption {
		PRIVATE_CHAT = 1,
		IGNORE_USER,
		ALIAS
	};

	HMENU menu;
	LVITEM item;
	User *user;
	enum MenuOption menu_option;

	if (note->iItem < 0)
		return;

	item.mask = LVIF_PARAM;
	item.iItem = note->iItem;
	SendMessage(note->hdr.hwndFrom, LVM_GETITEM, 0, (intptr_t)&item);
	user = (User *)item.lParam;

	menu = CreatePopupMenu();
	AppendMenu(menu, 0, PRIVATE_CHAT, L"Private chat");
	AppendMenu(menu, user->ignore ? MF_CHECKED : 0, IGNORE_USER, L"Ignore");
	AppendMenu(menu, 0, ALIAS, L"Set alias");
	SetMenuDefaultItem(menu, 1, 0);
	ClientToScreen(note->hdr.hwndFrom, &note->ptAction);
	menu_option = TrackPopupMenuEx(menu, TPM_RETURNCMD, note->ptAction.x,
	    note->ptAction.y, GetParent(note->hdr.hwndFrom), NULL);
	DestroyMenu(menu);

	switch (menu_option) {

	case PRIVATE_CHAT:
		ChatTab_focus_private(user);
		return;

	case IGNORE_USER:
		user->ignore = !user->ignore;
		return;

	case ALIAS: {
		char title[128];
		char buf[MAX_NAME_LENGTH_NUL];

		sprintf(title, "Set alias for %s", user->name);
		if (!GetTextDialog_create(title, strcpy(buf, UNTAGGED_NAME(user->name)), MAX_NAME_LENGTH_NUL))
			strcpy(user->alias, buf);
		return;
	}
	}
}

static intptr_t CALLBACK
chatlist_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	switch (msg) {

	case WM_CREATE:
		on_create(window);
		return 0;

	case WM_SIZE:
		on_size(window, LOWORD(l_param), HIWORD(l_param));
		return 0;

	case WM_NOTIFY: {
		NMHDR *note;

		note = (NMHDR *)l_param;

		if (note->idFrom != DLG_LIST)
			break;

		if (note->code == LVN_ITEMACTIVATE) {
			on_activate((NMITEMACTIVATE *)l_param);
			return 0;
		}
		if (note->code == NM_RCLICK) {
			on_right_click((NMITEMACTIVATE *)l_param);
			return 1;
		}
	}

	default:
		break;
	}

	return DefWindowProc(window, msg, w_param, l_param);
}

static void __attribute__((constructor))
_init (void)
{
	WNDCLASSEX window_class = {
		.lpszClassName = WC_CHATLIST,
		.cbSize        = sizeof window_class,
		.lpfnWndProc   = (WNDPROC)chatlist_proc,
		.hCursor       = LoadCursor(NULL, (wchar_t *)IDC_ARROW),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
	};

	RegisterClassEx(&window_class);
}

static void
update_user(HWND window, const User *u, int index)
{
	char *name;
	LVITEMA item;
	int icon_index;

	if (!strcmp(UNTAGGED_NAME(u->name), u->alias)) {
		name = (char *)u->name;

	} else {
		name = _alloca(MAX_NAME_LENGTH * 2 + sizeof " ()");
		sprintf(name, "%s (%s)", u->name, u->alias);
	}

	item.mask = LVIF_IMAGE | LVIF_STATE | LVIF_TEXT;
	item.iItem = index;
	item.iSubItem = 0;
	item.pszText = name;
	item.iImage = u->ingame ? ICON_INGAME
	            : u->battle ? ICONMASK_INGAME
	            : -1;
	item.stateMask = LVIS_OVERLAYMASK;

	icon_index = ICONMASK_USER;
	if (u->away)
		icon_index |= ICONMASK_AWAY;
	if (u->ignore)
		icon_index |= ICONMASK_IGNORE;

	item.state = INDEXTOOVERLAYMASK(icon_index);

	SendMessage(window, LVM_SETITEMA, 0, (intptr_t)&item);
}

void
ChatList_add_user(HWND window, const User *user)
{
	HWND list;
	int index;

	list = GetDlgItem(window, DLG_LIST);

	{ /* Insert new item to list */
		LVITEMA item;

		item.mask = LVIF_PARAM | LVIF_TEXT;
		item.iSubItem = 0;
		item.pszText = (char *)user->name;
		item.lParam = (intptr_t)user;
		index = SendMessage(list, LVM_INSERTITEMA, 0, (intptr_t)&item);
	}

	update_user(list, user, index);

	{ /* Set the country icon */
		LVITEM item;

		item.mask = LVIF_IMAGE;
		item.iItem = index;
		item.iSubItem = COLUMN_COUNTRY;
		item.iImage = ICON_FIRST_FLAG + user->country;
		SendMessage(list, LVM_SETITEM, 0, (intptr_t)&item);
	}

	{ /* Set the width of the columns */
		RECT rect;

		GetClientRect(list, &rect);
		SendMessage(list, LVM_SETCOLUMNWIDTH, 0, rect.right - ICON_SIZE);
		SendMessage(list, LVM_SETCOLUMNWIDTH, 1, ICON_SIZE);
	}
}

void
ChatList_remove_user(HWND window, const User *user)
{
	HWND list;
	LVFINDINFO info;
	int index;

	list = GetDlgItem(window, DLG_LIST);

	info.flags = LVFI_PARAM;
	info.lParam = (intptr_t)user;

	index = SendMessage(window, LVM_FINDITEM, (uintptr_t)-1,
	    (intptr_t)&info);
	SendMessage(list, LVM_DELETEITEM, (uintptr_t)index, 0);
}

/* void */
/* ChatList_update_user(HWND window, const User *user) */
/* { */
	/* HWND list; */
	/* LVFINDINFO info; */
	/* int index; */

	/* list = GetDlgItem(window, DLG_LIST); */

	/* info.flags = LVFI_PARAM; */
	/* info.lParam = (intptr_t)user; */

	/* index = SendMessage(window, LVM_FINDITEM, (uintptr_t)-1, */
	    /* (intptr_t)&info); */
	/* update_user(list, user, index); */
/* } */
