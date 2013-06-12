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
#include <stddef.h>
#include <stdio.h>

#include <windows.h>
#include <Commctrl.h>

#include "battle.h"
#include "battlelist.h"
#include "chat.h"
#include "chat_window.h"
#include "client_message.h"
#include "common.h"
#include "downloader.h"
#include "iconlist.h"
#include "mybattle.h"
#include "sync.h"
#include "user.h"
#include "wincommon.h"

HWND g_battle_list;

#define LENGTH(x) (sizeof(x) / sizeof(*x))

enum DialogId {
	DLG_LIST,
	DLG_MAP,
	DLG_BATTLE_INFO,
	DLG_BATTLE_LIST,
	DLG_JOIN,
	DLG_LAST = DLG_JOIN,
};

static const DialogItem dialog_items[] = {
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

static const wchar_t *const column_titles[] = {L"host", L"description", L"mod", L"map", L"Players"};

static LRESULT CALLBACK battlelist_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
static int CALLBACK compare_battle(const Battle *b1, const Battle *b2, int sort_order);
static int get_icon_index(const Battle *b);
static int get_index_from_battle(const Battle *b);
static Battle * get_battle_from_index(int index);
static void __attribute__((constructor)) init(void);
static void on_create(HWND window);
static void on_get_info_tip(NMLVGETINFOTIP *info);
static void on_item_right_click(POINT pt);
static void resize_columns(void);
static void set_item_text(int index, int column, const char *text);
static void sort_listview(int new_order);

static int CALLBACK
compare_battle(const Battle *b1, const Battle *b2, int sort_order)
{
	if (sort_order < 0) {
		const Battle *swap = b1;

		b1 = b2;
		b2 = swap;

		sort_order = -sort_order;
	}

	switch (sort_order) {

	case 1:
		return _stricmp(b1->founder->name, b2->founder->name);

	case 2:
		return _stricmp(b1->title, b2->title);

	case 3:
		return _stricmp(b1->mod_name, b2->mod_name);

	case 4:
		return _stricmp(b1->map_name, b2->map_name);

	case 5:
		return GetNumPlayers(b2) - GetNumPlayers(b1);

	default:
		assert(0);
		return 0;
	}
}

static void
sort_listview(int new_order)
{
	static int sort_order = 5;

	if (new_order)
		sort_order = new_order == sort_order ? -sort_order : new_order;

	ListView_SortItems(GetDlgItem(g_battle_list, DLG_LIST), compare_battle, sort_order);
}

static void
resize_columns(void)
{
	HWND list;
	RECT rect;
	int column_rem;
	int column_width;

	list = GetDlgItem(g_battle_list, DLG_LIST);
	GetClientRect(list, &rect);

	column_rem = rect.right % LENGTH(column_titles);
	column_width = rect.right / LENGTH(column_titles);

	for (int i=0, n = LENGTH(column_titles); i < n; ++i)
		ListView_SetColumnWidth(list, i, column_width + !i * column_rem);
}

static Battle *
get_battle_from_index(int index)
{
	LVITEM info;

	info.mask = LVIF_PARAM;
	info.iItem = index;
	info.iSubItem = 0;

	SendDlgItemMessage(g_battle_list, DLG_LIST, LVM_GETITEM, 0, (LPARAM)&info);
	return (Battle *)info.lParam;
}

static void
on_item_right_click(POINT pt)
{
	enum MenuId {
		FAIL, JOIN, DL_MAP, DL_MOD,
	};

	Battle *b;
	int index;
	HMENU menu;
	HMENU user_menu;
	enum MenuId item_clicked;

	index = SendDlgItemMessage(g_battle_list, DLG_LIST,
			LVM_SUBITEMHITTEST, 0,
			(LPARAM)&(LVHITTESTINFO){.pt = pt});

	if (index < 0)
		return;

	b = get_battle_from_index(index);

	menu = CreatePopupMenu();
	user_menu = CreatePopupMenu();

	AppendMenu(menu, 0, JOIN, L"Join battle");
	SetMenuDefaultItem(menu, JOIN, 0);
	AppendMenu(menu, MF_POPUP, (UINT_PTR )user_menu, L"Chat with ...");

	if (!Sync_map_hash(b->map_name))
		AppendMenu(menu, 0, DL_MAP, L"Download map");

	if (!Sync_mod_hash(b->mod_name))
		AppendMenu(menu, 0, DL_MOD, L"Download mod");

	for (uint8_t i = 0; i < g_my_battle->user_len - g_my_battle->bot_len; ++i)
		AppendMenuA(user_menu, 0, (UINT_PTR)g_my_battle->users[i],
				g_my_battle->users[i]->name);

	InsertMenu(user_menu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	ClientToScreen(g_battle_list, &pt);

	item_clicked = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y,
			g_battle_list, NULL);

	switch (item_clicked) {
	case FAIL:
		break;
	case JOIN:
		JoinBattle(b->id, NULL);
		break;
	case DL_MAP:
		DownloadMap(b->map_name);
		break;
	case DL_MOD:
		DownloadMod(b->mod_name);
		break;
	default:
		ChatWindow_set_active_tab(Chat_get_private_window((User *)item_clicked));
		break;
	}

	DestroyMenu(user_menu);
	DestroyMenu(menu);
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

	g_battle_list = window;
	CreateDlgItems(window, dialog_items, DLG_LAST + 1);
	list = GetDlgItem(g_battle_list, DLG_LIST);

	for (int i=0, n=sizeof(column_titles) / sizeof(char *); i < n; ++i) {
		LVCOLUMN info;
		info.mask = LVCF_TEXT | LVCF_SUBITEM;
		info.pszText = (wchar_t *)column_titles[i];
		info.iSubItem = i;
		ListView_InsertColumn(list, i, &info);
	}

	IconList_enable_for_listview(list);
	ListView_SetExtendedListViewStyle(list,
			LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP
			| LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
}

static LRESULT CALLBACK
battlelist_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch(msg) {
	case WM_CLOSE:
		return 0;
	case WM_CREATE:
		on_create(window);
		return 0;
	case WM_SIZE:
		MoveWindow(GetDlgItem(window, DLG_LIST), 0, 0, LOWORD(l_param), HIWORD(l_param), TRUE);
		resize_columns();
		return 0;
	case WM_NOTIFY:
		switch (((LPNMHDR)l_param)->code) {

		case LVN_COLUMNCLICK:
			sort_listview(((NMLISTVIEW *)l_param)->iSubItem + 1);
			return 0;
		case LVN_ITEMACTIVATE:
			JoinBattle(get_battle_from_index(((LPNMITEMACTIVATE)l_param)->iItem)->id, NULL);
			return 0;
		case NM_RCLICK:
			on_item_right_click(((LPNMITEMACTIVATE)l_param)->ptAction);
			return 1;
		case LVN_GETINFOTIP:
			on_get_info_tip((void *)l_param);
			return 0;
		}
		break;
	}
	return DefWindowProc(window, msg, w_param, l_param);
}

static int
get_index_from_battle(const Battle *b)
{
	LVFINDINFO info;

	info.flags = LVFI_PARAM;
	info.lParam = (LPARAM)b;

	return SendDlgItemMessage(g_battle_list, DLG_LIST, LVM_FINDITEM, -1,
			(LPARAM)&info);
}

void
BattleList_close_battle(const Battle *b)
{
	SendDlgItemMessage(g_battle_list, DLG_LIST, LVM_DELETEITEM,
			get_index_from_battle(b), 0);
}

void
BattleList_add_battle(Battle *b)
{
	LVITEM item;

	assert(get_index_from_battle(b) < 0);

	item.mask = LVIF_PARAM | LVIF_TEXT;
	item.iItem = 0;
	item.iSubItem = 0;
	item.lParam = (LPARAM)b;
	item.pszText = utf8to16(b->founder->name);

	__attribute__((unused))
	int index = SendDlgItemMessage(g_battle_list, DLG_LIST, LVM_INSERTITEM,
			0, (LPARAM)&item);

	assert(index >= 0);
}

static void
set_item_text(int index, int column, const char *text)
{
	LVITEM item;

	item.mask = LVIF_TEXT;
	item.iItem = index;
	item.iSubItem = column;
	item.pszText = utf8to16(text);
	SendDlgItemMessage(g_battle_list, DLG_LIST, LVM_SETITEM, 0,
			(LPARAM)&item);
}

static int
get_icon_index(const Battle *b)
{
	int icon_index = 0;

	if (b->founder->ingame)
		icon_index |= INGAME_MASK;
	if (b->passworded)
		icon_index |= PW_MASK;
	if (!b->locked && b->max_players == GetNumPlayers(b))
		icon_index |= FULL_MASK;

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
	SendDlgItemMessage(g_battle_list, DLG_LIST, LVM_SETITEM, 0,
			(LPARAM)&item);

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
init(void)
{
	WNDCLASSEX class_info = {
		.lpszClassName = WC_BATTLELIST,
		.cbSize        = sizeof(WNDCLASSEX),
		.lpfnWndProc   = battlelist_proc,
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};

	RegisterClassEx(&class_info);
}

void
BattleList_reset(void)
{
	SendDlgItemMessage(g_battle_list, DLG_LIST, LVM_DELETEALLITEMS, 0, 0);
}
