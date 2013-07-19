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
#include <stddef.h>
#include <stdio.h>

#include <windows.h>
#include <commctrl.h>

#include "battle.h"
#include "battlelist.h"
#include "chatbox.h"
#include "chattab.h"
#include "tasserver.h"
#include "common.h"
#include "iconlist.h"
#include "mybattle.h"
#include "sync.h"
#include "user.h"
#include "wincommon.h"

#define LENGTH(x) (sizeof x / sizeof *x)

enum DialogId {
	DLG_LIST,
	DLG_MAP,
	DLG_BATTLE_INFO,
	DLG_BATTLE_LIST,
	DLG_JOIN,
	DLG_LAST = DLG_JOIN,
};

static void             _init           (void);
static intptr_t CALLBACK battlelist_proc (HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param);
static int CALLBACK     compare_battle  (const Battle *b1, const Battle *b2, int sort_order);
static int              get_icon_index  (const Battle *b);
static intptr_t         get_index_from_battle(const Battle *b);
static Battle *         get_battle_from_index(int index);
static void             on_create       (HWND window);
static void             on_get_info_tip (NMLVGETINFOTIP *info);
static void             on_item_right_click(POINT point);
static void             resize_columns  (void);
static void             set_item_text   (int index, int column, const char *text);
static void             sort_listview   (int new_order);

static const DialogItem DIALOG_ITEMS[] = {
	[DLG_LIST] = {
		.class = WC_LISTVIEW,
		.style = WS_VISIBLE | LVS_REPORT | LVS_SHAREIMAGELISTS | LVS_SINGLESEL,
	}, [DLG_MAP] = {
		.class = WC_STATIC,
		.style = WS_VISIBLE,
	}, [DLG_BATTLE_INFO] = {
		.class = WC_STATIC,
		.style = WS_VISIBLE,
	}, [DLG_BATTLE_LIST] = {
		.class = WC_LISTVIEW,
		.style = WS_VISIBLE | LVS_REPORT,
	}, [DLG_JOIN] = {
		.class = WC_BUTTON,
		.style = WS_VISIBLE,
	},
};

static const wchar_t *const COLUMN_TITLES[] = {L"host", L"description", L"mod", L"map", L"Players"};
static HWND s_battle_list;

static int CALLBACK
compare_battle(const Battle *b1, const Battle *b2, int sort_order)
{
	if (sort_order < 0) {
		const Battle *swap;

		swap = b1;
		b1 = b2;
		b2 = swap;

		sort_order = -sort_order;
	}

	switch (sort_order) {
	case 1:  return _stricmp(b1->founder->name, b2->founder->name);
	case 2:  return _stricmp(b1->title, b2->title);
	case 3:  return _stricmp(b1->mod_name, b2->mod_name);
	case 4:  return _stricmp(b1->map_name, b2->map_name);
	case 5:  return GetNumPlayers(b2) - GetNumPlayers(b1);
	default: assert(0); return 0;
	}
}

static void
sort_listview(int new_order)
{
	static int sort_order = 5;

	if (new_order)
		sort_order = new_order == sort_order ? -sort_order : new_order;

	ListView_SortItems(GetDlgItem(s_battle_list, DLG_LIST), compare_battle, sort_order);
}

static void
resize_columns(void)
{
	HWND list;
	RECT rect;
	int32_t column_rem;
	int32_t column_width;

	list = GetDlgItem(s_battle_list, DLG_LIST);
	GetClientRect(list, &rect);

	column_rem = rect.right % (int32_t)LENGTH(COLUMN_TITLES);
	column_width = rect.right / (int32_t)LENGTH(COLUMN_TITLES);

	for (size_t i = 0; i < LENGTH(COLUMN_TITLES); ++i)
		ListView_SetColumnWidth(list, i, column_width + !i * column_rem);
}

static Battle *
get_battle_from_index(int index)
{
	LVITEM info;

	info.mask = LVIF_PARAM;
	info.iItem = index;
	info.iSubItem = 0;

	SendDlgItemMessage(s_battle_list, DLG_LIST, LVM_GETITEM, 0, (intptr_t)&info);
	return (Battle *)info.lParam;
}

static void
on_item_right_click(POINT point)
{
	enum MenuId {
		FAIL, JOIN, DL_MAP, DL_MOD,
	};

	Battle *b;
	int index;
	HMENU menu;
	HMENU user_menu;
	int item_clicked;
	LVHITTESTINFO hit_test;

	hit_test.pt = point;

	index = SendDlgItemMessage(s_battle_list, DLG_LIST,
			LVM_SUBITEMHITTEST, 0, (intptr_t)&hit_test);

	if (index < 0)
		return;

	b = get_battle_from_index(index);

	menu = CreatePopupMenu();
	user_menu = CreatePopupMenu();

	AppendMenu(menu, 0, JOIN, L"Join battle");
	SetMenuDefaultItem(menu, JOIN, 0);
	AppendMenu(menu, MF_POPUP, (uintptr_t)user_menu, L"Chat with ...");

	if (!Sync_map_hash(b->map_name))
		AppendMenu(menu, 0, DL_MAP, L"Download map");

	if (!Sync_mod_hash(b->mod_name))
		AppendMenu(menu, 0, DL_MOD, L"Download mod");

	for (uint8_t i = 0; i < b->user_len - b->bot_len; ++i)
		AppendMenuA(user_menu, 0, (uintptr_t)b->users[i],
				b->users[i]->name);

	InsertMenu(user_menu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	ClientToScreen(s_battle_list, &point);

	item_clicked = TrackPopupMenuEx(menu, TPM_RETURNCMD, point.x, point.y,
			s_battle_list, NULL);

	DestroyMenu(user_menu);
	DestroyMenu(menu);

	switch (item_clicked) {
	case FAIL:
		return;

	case JOIN:
		TasServer_send_join_battle(b->id, NULL);
		return;

	case DL_MAP:
		/* DownloadMap(b->map_name); */
		return;

	case DL_MOD:
		/* DownloadMod(b->mod_name); */
		return;

	default:
		ChatTab_focus_private((User *)item_clicked);
		return;
	}
}

static void
on_get_info_tip(NMLVGETINFOTIP *info)
{
	Battle *b;

	b = get_battle_from_index(info->iItem);
	_swprintf(info->pszText,
			L"%hs\n%hs\n%hs\n%s\n%d/%d players - %d spectators",
			b->founder->name, b->mod_name, b->map_name,
			utf8to16(b->title), GetNumPlayers(b), b->max_players,
			b->user_len);
}

static void
on_create(HWND window)
{
	HWND list;

	s_battle_list = window;
	CreateDlgItems(window, DIALOG_ITEMS, DLG_LAST + 1);
	list = GetDlgItem(s_battle_list, DLG_LIST);

	for (int i=0, n=sizeof COLUMN_TITLES / sizeof *COLUMN_TITLES; i < n; ++i) {
		LVCOLUMN info;
		info.mask = LVCF_TEXT | LVCF_SUBITEM;
		info.pszText = (wchar_t *)COLUMN_TITLES[i];
		info.iSubItem = i;
		ListView_InsertColumn(list, i, &info);
	}

	IconList_enable_for_listview(list);
	ListView_SetExtendedListViewStyle(list,
			LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP
			| LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
}

static intptr_t CALLBACK
battlelist_proc(HWND window, uint32_t msg, uintptr_t w_param, intptr_t l_param)
{
	switch(msg) {

	case WM_CLOSE:
		return 0;

	case WM_CREATE:
		on_create(window);
		return 0;

	case WM_SIZE:
		MoveWindow(GetDlgItem(window, DLG_LIST), 0, 0, LOWORD(l_param), HIWORD(l_param), TRUE);
		return 0;

	case WM_EXITSIZEMOVE:
		resize_columns();
		return 0;

	case WM_NOTIFY:
		switch (((NMHDR *)l_param)->code) {

		case LVN_COLUMNCLICK:
			sort_listview(((NMLISTVIEW *)l_param)->iSubItem + 1);
			return 0;

		case LVN_ITEMACTIVATE:
			TasServer_send_join_battle(get_battle_from_index(((NMITEMACTIVATE *)l_param)->iItem)->id, NULL);
			return 0;

		case NM_RCLICK:
			on_item_right_click(((NMITEMACTIVATE *)l_param)->ptAction);
			return 1;

		case LVN_GETINFOTIP:
			on_get_info_tip((void *)l_param);
			return 0;
		}
		break;
	}
	return DefWindowProc(window, msg, w_param, l_param);
}

static intptr_t
get_index_from_battle(const Battle *b)
{
	LVFINDINFO info;

	info.flags = LVFI_PARAM;
	info.lParam = (intptr_t)b;

	return SendDlgItemMessage(s_battle_list, DLG_LIST, LVM_FINDITEM,
	    (uintptr_t)-1, (intptr_t)&info);
}

void
BattleList_close_battle(const Battle *b)
{
	SendDlgItemMessage(s_battle_list, DLG_LIST, LVM_DELETEITEM,
			(uintptr_t)get_index_from_battle(b), 0);
}

void
BattleList_add_battle(Battle *b)
{
	LVITEM item;

	assert(get_index_from_battle(b) < 0);

	item.mask = LVIF_PARAM | LVIF_TEXT | LVIF_IMAGE;
	item.iItem = 0;
	item.iSubItem = 0;
	item.iImage = -1;
	item.lParam = (intptr_t)b;
	item.pszText = utf8to16(b->founder->name);

	SendDlgItemMessage(s_battle_list, DLG_LIST, LVM_INSERTITEM,
			0, (intptr_t)&item);
}

static void
set_item_text(int index, int column, const char *text)
{
	LVITEM item;

	item.mask = LVIF_TEXT;
	item.iItem = index;
	item.iSubItem = column;
	item.pszText = utf8to16(text);
	SendDlgItemMessage(s_battle_list, DLG_LIST, LVM_SETITEM, 0,
			(intptr_t)&item);
}

static int
get_icon_index(const Battle *b)
{
	int icon_index = 0;

	if (b->founder->ingame)
		icon_index |= ICONMASK_INGAME;
	if (b->passworded)
		icon_index |= ICONMASK_PASSWORD;
	if (!b->locked && b->max_players == GetNumPlayers(b))
		icon_index |= ICONMASK_FULL;

	return icon_index;
}

void
BattleList_update_battle(const Battle *b)
{
	int index;
	LVITEM item;
	char buf[16];

	index = get_index_from_battle(b);
	assert(index >= 0);

	item.mask = LVIF_STATE | LVIF_IMAGE;
	item.iItem = index;
	item.iSubItem = 0;
	item.iImage = b->locked ? ICON_CLOSED : ICON_OPEN;
	item.stateMask = LVIS_OVERLAYMASK;
	item.state = INDEXTOOVERLAYMASK(get_icon_index(b));
	SendDlgItemMessage(s_battle_list, DLG_LIST, LVM_SETITEM, 0,
			(intptr_t)&item);

	set_item_text(index, 1, b->title);
	set_item_text(index, 2, b->mod_name);
	set_item_text(index, 3, b->map_name);

	sprintf(buf, "%d / %d +%d",
			GetNumPlayers(b), b->max_players, b->spectator_len);
	set_item_text(index, 4, buf);

	sort_listview(0);
}


void
BattleList_on_end_login_info(void)
{
	sort_listview(0);
	resize_columns();
}

static void __attribute__((constructor))
_init(void)
{
	WNDCLASSEX class_info = {
		.lpszClassName = WC_BATTLELIST,
		.cbSize        = sizeof class_info,
		.lpfnWndProc   = (WNDPROC)battlelist_proc,
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
	};

	RegisterClassEx(&class_info);
}

void
BattleList_reset(void)
{
	SendDlgItemMessage(s_battle_list, DLG_LIST, LVM_DELETEALLITEMS, 0, 0);
}
