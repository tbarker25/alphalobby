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
sort_listview(int newOrder)
{
	static int
sortOrder = 5, reverseSort = 0;
	reverseSort = newOrder == sortOrder && !reverseSort;

	sortOrder = newOrder ?: sortOrder;

	int CALLBACK CompareFunc(const Battle *b1, const Battle *b2, int unused)
	{
		if (reverseSort) {
			const Battle *swap = b1;
			b1 = b2;
			b2 = swap;
		}
		if (sortOrder == 5)
			return GetNumPlayers(b2) - GetNumPlayers(b1);
		char *s1, *s2;
		if (sortOrder == 1) {
			s1 = b1->founder->name;
			s2 = b2->founder->name;
		} else {
			const size_t offsets[] = {
				[2] = offsetof(Battle, title),
				[3] = offsetof(Battle, mod_name),
				[4] = offsetof(Battle, map_name),
			};
			s1 = (void *)b1 + offsets[sortOrder];
			s2 = (void *)b2 + offsets[sortOrder];
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

	int columnRem = rect.right % LENGTH(columns);
	int columnWidth = rect.right / LENGTH(columns);

	for (int i=0, n = LENGTH(columns); i < n; ++i)
		ListView_SetColumnWidth(list, i, columnWidth + !i * columnRem);
}

static Battle * getBattleFromIndex(int index) {
	LVITEM item = {
		.mask = LVIF_PARAM,
		.iItem = index
	};
	SendDlgItemMessage(g_battle_list, DLG_LIST, LVM_GETITEM, 0, (LPARAM)&item);
	return (Battle *)item.lParam;
}

static void
onItemRightClick(POINT pt)
{
	int index = SendDlgItemMessage(g_battle_list, DLG_LIST,
			LVM_SUBITEMHITTEST, 0,
			(LPARAM)&(LVHITTESTINFO){.pt = pt});

	if (index < 0)
		return;

	Battle *b = getBattleFromIndex(index);

	enum {
		JOIN = 1, DL_MAP, DL_MOD,
	};

	HMENU menu = CreatePopupMenu();
	AppendMenu(menu, 0, JOIN, L"Join battle");
	SetMenuDefaultItem(menu, JOIN, 0);
	HMENU userMenu = CreatePopupMenu();
	AppendMenu(menu, MF_POPUP, (UINT_PTR )userMenu, L"Chat with ...");
	if (!Sync_map_hash(b->map_name))
		AppendMenu(menu, 0, DL_MAP, L"Download map");
	if (!Sync_mod_hash(b->mod_name))
		AppendMenu(menu, 0, DL_MOD, L"Download mod");


	FOR_EACH_USER(u, b)
		AppendMenuA(userMenu, 0, (UINT_PTR)u, u->name);

	InsertMenu(userMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
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

	DestroyMenu(userMenu);
	DestroyMenu(menu);
}

static void
onGetInfoTip(NMLVGETINFOTIP *info)
{
	Battle *b = getBattleFromIndex(info->iItem);
	_swprintf(info->pszText,
			L"%hs\n%hs\n%hs\n%s\n%d/%d players - %d spectators",
			b->founder->name, b->mod_name, b->map_name,
			utf8to16(b->title), GetNumPlayers(b), b->max_players,
			b->nbParticipants);
}

static void
onCreate(HWND window)
{
	g_battle_list = window;
	CreateDlgItems(window, dialog_items, DLG_LAST + 1);

	HWND listDlg = GetDlgItem(g_battle_list, DLG_LIST);

	LVCOLUMN columnInfo = { LVCF_TEXT | LVCF_SUBITEM };
	for (int i=0, n=sizeof(columns) / sizeof(char *); i < n; ++i) {
		columnInfo.pszText = (wchar_t *)columns[i];
		columnInfo.iSubItem = i;
		ListView_InsertColumn(listDlg, i, &columnInfo);
	}

	EnableIconList(listDlg);
	ListView_SetExtendedListViewStyle(listDlg,
			LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP
			| LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
}

static LRESULT CALLBACK battleList_proc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CLOSE:
		return 0;
	case WM_CREATE:
		onCreate(window);
		return 0;
	case WM_SIZE:
		MoveWindow(GetDlgItem(window, DLG_LIST), 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		resize_columns();
		return 0;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {

		case LVN_COLUMNCLICK:
			sort_listview(((NMLISTVIEW *)lParam)->iSubItem + 1);
			return 0;
		case LVN_ITEMACTIVATE:
			JoinBattle(getBattleFromIndex(((LPNMITEMACTIVATE)lParam)->iItem)->id, NULL);
			return 0;
		case NM_RCLICK:
			onItemRightClick(((LPNMITEMACTIVATE)lParam)->ptAction);
			return 1;
		case LVN_GETINFOTIP:
			onGetInfoTip((void *)lParam);
			return 0;
		}
		break;
	}
	return DefWindowProc(window, msg, wParam, lParam);
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

	size_t iconIndex = 0;
	if (b->founder->client_status & CS_INGAME)
		iconIndex |= INGAME_MASK;
	if (b->passworded)
		iconIndex |= PW_MASK;
	if (!b->locked && b->max_players == GetNumPlayers(b))
		iconIndex |= FULL_MASK;
	item.state = INDEXTOOVERLAYMASK(iconIndex);

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
			GetNumPlayers(b), b->max_players, b->nbSpectators);
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

static void
__attribute__((constructor))
init (void)
{
	WNDCLASSEX classInfo = {
		.lpszClassName = WC_BATTLELIST,
		.cbSize        = sizeof(WNDCLASSEX),
		.lpfnWndProc   = battleList_proc,
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};
	RegisterClassEx(&classInfo);
}

void
BattleList_reset(void)
{
	SendDlgItemMessage(g_battle_list, DLG_LIST, LVM_DELETEALLITEMS, 0, 0);
}
