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


enum DLG_ID {
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

static const wchar_t *const columns[] = {L"host", L"description", L"mod", L"map", L"Players"};

static void
sort_listview(int new_order)
{
	static int
sort_order = 5, reverse_sort = 0;
	reverse_sort = new_order == sort_order && !reverse_sort;

	sort_order = new_order ?: sort_order;

	int CALLBACK CompareFunc(const Battle *b1, const Battle *b2,
			__attribute__((unused)) int unused)
	{
		if (reverse_sort) {
			const Battle *swap = b1;
			b1 = b2;
			b2 = swap;
		}
		if (sort_order == 5)
			return GetNumPlayers(b2) - GetNumPlayers(b1);
		char *s1, *s2;
		if (sort_order == 1) {
			s1 = b1->founder->name;
			s2 = b2->founder->name;
		} else {
			const size_t offsets[] = {
				[2] = offsetof(Battle, title),
				[3] = offsetof(Battle, mod_name),
				[4] = offsetof(Battle, map_name),
			};
			s1 = (void *)b1 + offsets[sort_order];
			s2 = (void *)b2 + offsets[sort_order];
		}
		return _stricmp(s1, s2);
	}

	ListView_SortItems(GetDlgItem(g_battle_list, DLG_LIST), CompareFunc, 0);
}

static void
resize_columns(void)
{
	RECT rect;
	HWND list = GetDlgItem(g_battle_list, DLG_LIST);
	GetClientRect(list, &rect);

	int column_rem = rect.right % LENGTH(columns);
	int column_width = rect.right / LENGTH(columns);

	for (int i=0, n = LENGTH(columns); i < n; ++i)
		ListView_SetColumnWidth(list, i, column_width + !i * column_rem);
}

static Battle *
get_battle_from_index(int index) {
	LVITEM item = {
		.mask = LVIF_PARAM,
		.iItem = index
	};
	SendDlgItemMessage(g_battle_list, DLG_LIST, LVM_GETITEM, 0, (LPARAM)&item);
	return (Battle *)item.lParam;
}

static void
on_item_right_click(POINT pt)
{
	int index = SendDlgItemMessage(g_battle_list, DLG_LIST,
			LVM_SUBITEMHITTEST, 0,
			(LPARAM)&(LVHITTESTINFO){.pt = pt});

	if (index < 0)
		return;

	Battle *b = get_battle_from_index(index);

	enum {
		JOIN = 1, DL_MAP, DL_MOD,
	};

	HMENU menu = CreatePopupMenu();
	AppendMenu(menu, 0, JOIN, L"Join battle");
	SetMenuDefaultItem(menu, JOIN, 0);
	HMENU user_menu = CreatePopupMenu();
	AppendMenu(menu, MF_POPUP, (UINT_PTR )user_menu, L"Chat with ...");
	if (!Sync_map_hash(b->map_name))
		AppendMenu(menu, 0, DL_MAP, L"Download map");
	if (!Sync_mod_hash(b->mod_name))
		AppendMenu(menu, 0, DL_MOD, L"Download mod");


	FOR_EACH_USER(u, b)
		AppendMenuA(user_menu, 0, (UINT_PTR)u, u->name);

	InsertMenu(user_menu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	ClientToScreen(g_battle_list, &pt);

	int clicked = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y,
			g_battle_list, NULL);
	switch (clicked) {
	case 0:
		break;
	case 1:
		JoinBattle(b->id, NULL);
		break;
	case DL_MAP:
		DownloadMap(b->map_name);
		break;
	case DL_MOD:
		DownloadMod(b->mod_name);
		break;
	default:
		ChatWindow_set_active_tab(Chat_get_private_window((User *)clicked));
		break;
	}

	DestroyMenu(user_menu);
	DestroyMenu(menu);
}

static void
on_get_info_tip(NMLVGETINFOTIP *info)
{
	Battle *b = get_battle_from_index(info->iItem);
	_swprintf(info->pszText,
			L"%hs\n%hs\n%hs\n%s\n%d/%d players - %d spectators",
			b->founder->name, b->mod_name, b->map_name,
			utf8to16(b->title), GetNumPlayers(b), b->max_players,
			b->participant_len);
}

static void
on_create(HWND window)
{
	g_battle_list = window;
	CreateDlgItems(window, dialog_items, DLG_LAST + 1);

	HWND list_dlg = GetDlgItem(g_battle_list, DLG_LIST);

	for (int i=0, n=sizeof(columns) / sizeof(char *); i < n; ++i) {
		LVCOLUMN info;
		info.mask = LVCF_TEXT | LVCF_SUBITEM;
		info.pszText = (wchar_t *)columns[i];
		info.iSubItem = i;
		ListView_InsertColumn(list_dlg, i, &info);
	}

	EnableIconList(list_dlg);
	ListView_SetExtendedListViewStyle(list_dlg,
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


void
BattleList_CloseBattle(Battle *b)
{
	LVFINDINFO find_info = {.flags = LVFI_PARAM, .lParam = (LPARAM)b};
	HWND list = GetDlgItem(g_battle_list, DLG_LIST);
	int index = ListView_FindItem(list, -1, &find_info);
	ListView_DeleteItem(list, index);
}

void
BattleList_UpdateBattle(Battle *b)
{
	HWND list = GetDlgItem(g_battle_list, DLG_LIST);
	LVFINDINFO find_info = {LVFI_PARAM, .lParam = (LPARAM)b};
	LVITEM item;
	item.iItem = ListView_FindItem(list, -1, &find_info);
	item.iSubItem = 0;

	if (item.iItem == -1) {
		item.iItem = 0;
		item.mask = LVIF_PARAM | LVIF_TEXT;
		item.lParam = (LPARAM)b;
		item.pszText = utf8to16(b->founder->name);

		item.iItem = SendMessage(list, LVM_INSERTITEM, 0, (LPARAM)&item);
	}

	item.mask = LVIF_STATE | LVIF_IMAGE;

	item.iImage = b->locked ? ICONS_CLOSED : ICONS_OPEN,
	item.stateMask = LVIS_OVERLAYMASK;

	size_t icon_index = 0;
	if (b->founder->client_status & CS_INGAME)
		icon_index |= INGAME_MASK;
	if (b->passworded)
		icon_index |= PW_MASK;
	if (!b->locked && b->max_players == GetNumPlayers(b))
		icon_index |= FULL_MASK;
	item.state = INDEXTOOVERLAYMASK(icon_index);

	ListView_SetItem(list, &item);

	item.mask = LVIF_TEXT;

#define ADD_STRING(s) \
	++item.iSubItem; \
	item.pszText = s; \
	SendMessage(list, LVM_SETITEM, 0, (LPARAM)&item); \

	ADD_STRING(utf8to16(b->title));
	ADD_STRING(utf8to16(b->mod_name));
	ADD_STRING(utf8to16(b->map_name));
	wchar_t buf[16];
	_swprintf(buf, L"%d / %d +%d",
			GetNumPlayers(b), b->max_players, b->spectator_len);
	ADD_STRING(buf);

#undef ADD_STRING

	sort_listview(0);
}

void
BattleList_OnEndLoginInfo(void)
{
	sort_listview(0);
	resize_columns();
}

static void __attribute__((constructor))
init (void)
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
