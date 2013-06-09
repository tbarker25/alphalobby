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
#include <limits.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>
#include <windowsx.h>
#include <Commctrl.h>
#include <richedit.h>
#include <commdlg.h>

#include "battle.h"
#include "battleroom.h"
#include "chat.h"
#include "chat_window.h"
#include "client_message.h"
#include "common.h"
#include "countrycodes.h"
#include "iconlist.h"
#include "layoutmetrics.h"
#include "mainwindow.h"
#include "mybattle.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "user.h"
#include "usermenu.h"
#include "wincommon.h"

HWND g_battle_room;

#define LENGTH(x) (sizeof(x) / sizeof(*x))

#define CFE_LINK		0x0020
// #define CFM_LINK		0x00000020
#define CFM_STYLE			0x00080000
#define CFM_KERNING			0x00100000

#define NUM_SIDE_BUTTONS 4

static const uint16_t *minimapPixels;
static uint16_t metalMapHeight, metalMapWidth;
static const uint8_t *metalMapPixels;
static uint16_t heightMapHeight, heightMapWidth;
static const uint8_t *heightMapPixels;

enum PLAYER_LIST_COLUMNS {
	COLUMN_STATUS,
	COLUMN_RANK,
	COLUMN_NAME,
	COLUMN_COLOR,
	COLUMN_SIDE,
	COLUMN_FLAG,
	COLUMN_LAST = COLUMN_FLAG,
};

enum DLG_ID {
	DLG_CHAT,
	DLG_ALLY_LABEL,
	DLG_PLAYERLIST,
	DLG_ALLY,
	DLG_SIDE_LABEL,
	DLG_SIDE_FIRST,
	DLG_SIDE_LAST = DLG_SIDE_FIRST + NUM_SIDE_BUTTONS,
	DLG_SPECTATE,
	DLG_AUTO_UNSPEC,
	DLG_MINIMAP,
	DLG_START,
	DLG_LEAVE,
	DLG_INFOLIST,


	DLG_SPLIT_FIRST,
	DLG_SPLIT_LAST = DLG_SPLIT_FIRST + SPLIT_LAST,

	DLG_SPLIT_SIZE,

	DLG_MAPMODE_MINIMAP,
	DLG_MAPMODE_METAL,
	DLG_MAPMODE_ELEVATION,
	DLG_CHANGE_MAP,

	DLG_LAST = DLG_CHANGE_MAP,
};

static const DialogItem dialog_items[] = {
	[DLG_CHAT] = {
		.class = WC_CHATBOX,
		.style = WS_VISIBLE,
	}, [DLG_ALLY] = {
		.class = WC_COMBOBOX,
		.style = WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
	}, [DLG_ALLY_LABEL] = {
		.class = WC_STATIC,
		.name = L"Team:",
		.style = WS_VISIBLE,
	}, [DLG_SIDE_FIRST ... DLG_SIDE_LAST] = {
		.class = WC_BUTTON,
		.style = WS_VISIBLE | BS_CHECKBOX | BS_PUSHLIKE | BS_ICON,
	}, [DLG_SIDE_LABEL] = {
		.class = WC_STATIC,
		.name = L"Side:",
		.style = WS_VISIBLE,
	}, [DLG_SPECTATE] = {
		.class = WC_BUTTON,
		.name = L"Spectate",
		.style = WS_VISIBLE | BS_CHECKBOX,
	}, [DLG_AUTO_UNSPEC] = {
		.class = WC_BUTTON,
		.name = L"Auto unspectate",
		.style = WS_VISIBLE | BS_AUTOCHECKBOX | WS_DISABLED,
	}, [DLG_MINIMAP] = {
		.class = WC_STATIC,
		.style = WS_VISIBLE | SS_OWNERDRAW,
	}, [DLG_LEAVE] = {
		.class = WC_BUTTON,
		.name = L"Leave",
		.style = WS_VISIBLE | BS_PUSHBUTTON,
	}, [DLG_START] = {
		.class = WC_BUTTON,
		.name = L"Start",
		.style = WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
	}, [DLG_PLAYERLIST] = {
		.class = WC_LISTVIEW,
		.exStyle = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | LVS_SHAREIMAGELISTS | LVS_SINGLESEL | LVS_REPORT | LVS_NOCOLUMNHEADER,
	}, [DLG_INFOLIST] = {
		.class = WC_LISTVIEW,
		.exStyle = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | LVS_SINGLESEL | LVS_REPORT | LVS_NOCOLUMNHEADER,

	}, [DLG_MAPMODE_MINIMAP] = {
		.class = WC_BUTTON,
		.name = L"Minimap",
		.style = WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE,
	}, [DLG_MAPMODE_METAL] = {
		.class = WC_BUTTON,
		.name = L"Metal",
		.style = WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE,
	}, [DLG_MAPMODE_ELEVATION] = {
		.class = WC_BUTTON,
		.name = L"Elevation",
		.style = WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE,
	}, [DLG_CHANGE_MAP] = {
		.class = WC_BUTTON,
		.name = L"Change Map",
		.style = WS_VISIBLE | BS_PUSHBUTTON,

	}, [DLG_SPLIT_FIRST ... DLG_SPLIT_LAST] = {
		.class = WC_BUTTON,
		.style = WS_VISIBLE | BS_CHECKBOX | BS_PUSHLIKE | BS_ICON,

	}, [DLG_SPLIT_SIZE] = {
		.class = TRACKBAR_CLASS,
		.style = WS_VISIBLE | WS_CHILD,
	}
};

void
BattleRoom_change_minimap_bitmap(const uint16_t *_minimapPixels,
		uint16_t _metalMapWidth, uint16_t _metalMapHeight, const uint8_t *_metalMapPixels,
		uint16_t _heightMapWidth, uint16_t _heightMapHeight, const uint8_t *_heightMapPixels)
{
	minimapPixels = _minimapPixels;

	metalMapWidth = _metalMapWidth;
	metalMapHeight = _metalMapHeight;
	metalMapPixels = _metalMapPixels;

	heightMapWidth = _heightMapWidth;
	heightMapHeight = _heightMapHeight;
	heightMapPixels = _heightMapPixels;

	InvalidateRect(GetDlgItem(g_battle_room, DLG_MINIMAP), 0, 0);
}

void
BattleRoom_redraw_minimap(void)
{
	InvalidateRect(GetDlgItem(g_battle_room, DLG_MINIMAP), 0, 0);
}

static void
set_split(SplitType type, int size)
{
	for (int i=0; i<=SPLIT_LAST; ++i)
		SendDlgItemMessage(g_battle_room, DLG_SPLIT_FIRST + i,
				BM_SETCHECK, i == type, 0);

	EnableWindow(GetDlgItem(g_battle_room, DLG_SPLIT_SIZE),
			g_battle_options.startPosType == STARTPOS_CHOOSE_INGAME);

	SendDlgItemMessage(g_battle_room, DLG_SPLIT_SIZE, TBM_SETPOS, 1, size);
}

void
BattleRoom_on_start_position_change(void)
{
	StartRect r1, r2;

	if (!battleInfoFinished)
		return;

	BattleRoom_redraw_minimap();

	if (g_battle_options.startPosType == STARTPOS_RANDOM) {
		set_split(SPLIT_RAND, 0);
		return;
	}

	if (g_battle_options.startPosType != STARTPOS_CHOOSE_INGAME) {
		set_split(SPLIT_LAST + 1, 0);
		return;
	}

	if (g_battle_options.startRects[0].left == 0
			&& (g_battle_options.startRects[0].top == 0
				|| g_battle_options.startRects[1].left != 0)) {
		r1 = g_battle_options.startRects[0];
		r2 = g_battle_options.startRects[1];
	} else {
		r1 = g_battle_options.startRects[1];
		r2 = g_battle_options.startRects[0];
	}

	if (r1.left == 0 && r1.top == 0 && r1.bottom == 200
			&& r2.top == 0 && r2.right == 200 && r2.bottom == 200
			&& r1.right == 200 - r2.left) {
		set_split(SPLIT_VERT, r1.right);
		return;
	}

	if (r1.left == 0 && r1.top == 0 && r1.right == 200
			&& r2.left == 0 && r2.right == 200 && r2.bottom == 200
			&& r1.bottom == 200 - r2.top) {
		set_split(SPLIT_HORZ, r1.bottom);
		return;
	}

	if (r1.left == 0 && r1.top == 0 && r2.right == 200 && r2.bottom == 200
			&& r1.right == 200 - r2.left
			&& r1.bottom == 200 - r2.top) {
		set_split(SPLIT_CORNERS1, (r1.right + r1.bottom) / 2);
		return;
	}

	if (r1.left == 0 && r1.bottom == 200 && r2.right == 200 && r2.top == 0
			&& r1.right == 200 - r2.left
			&& r1.top == 200 - r2.bottom) {
		set_split(SPLIT_CORNERS2, (r1.right + r2.bottom) / 2);
		return;
	}
}

void
BattleRoom_show(void)
{
	RECT rect;
	GetClientRect(g_battle_room, &rect);
	SendMessage(g_battle_room, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
	MainWindow_enable_battleroom_button();
}

void
BattleRoom_hide(void)
{
	ListView_DeleteAllItems(GetDlgItem(g_battle_room, DLG_PLAYERLIST));
	MainWindow_disable_battleroom_button();
}

static int
find_user(const void *u)
{
	LVFINDINFO find_info;
	find_info.flags = LVFI_PARAM;
	find_info.lParam = (LPARAM)u;
	return SendDlgItemMessage(g_battle_room, DLG_PLAYERLIST, LVM_FINDITEM,
			-1, (LPARAM)&find_info);
}

void
BattleRoom_on_left_battle(const union UserOrBot *s)
{
	SendDlgItemMessage(g_battle_room, DLG_PLAYERLIST, LVM_DELETEITEM, find_user(s), 0);
}

static const char *
get_effective_name(const union UserOrBot *s)
{
	if (s->battle_status & BS_AI)
		return s->bot.owner->name;
	return s->name;
}

static int CALLBACK
sort_listview(const union UserOrBot *u1, const union UserOrBot *u2, LPARAM unused)
{
	return _stricmp(get_effective_name(u1), get_effective_name(u2));
}

static void
update_group(int groupId)
{
	if (groupId >= 16)
		return;

	wchar_t buf[256];
	int playersOnTeam = 0;
	FOR_EACH_PLAYER(p, g_my_battle)
		playersOnTeam += FROM_BS_ALLY(p->battle_status) == groupId;
	_swprintf(buf, L"Team %d :: %d Player%c", groupId + 1, playersOnTeam, playersOnTeam > 1 ? 's' : '\0');

	LVGROUP groupInfo;
	groupInfo.cbSize = sizeof(groupInfo);
	groupInfo.mask = LVGF_HEADER;
	groupInfo.pszHeader = buf;
	SendDlgItemMessage(g_battle_room, DLG_PLAYERLIST, LVM_SETGROUPINFO,
			groupId, (LPARAM)&groupInfo);
}

bool
BattleRoom_is_auto_unspec(void)
{
	return SendDlgItemMessage(g_battle_room, DLG_AUTO_UNSPEC, BM_GETCHECK, 0, 0);
}

void
BattleRoom_update_user(union UserOrBot *s)
{
	HWND playerList = GetDlgItem(g_battle_room, DLG_PLAYERLIST);

	uint32_t battle_status = s->battle_status;
	int groupId = (battle_status & BS_MODE) ? FROM_BS_ALLY(battle_status) : 16;

	LVITEM item;
	item.iItem = find_user(s);
	item.iSubItem = 0;

	if (item.iItem == -1) {
		item.mask = LVIF_PARAM | LVIF_GROUPID,
		item.iItem = 0;
		item.lParam = (LPARAM)s;
		item.iGroupId = groupId;
		ListView_InsertItem(playerList, &item);

	} else {
		item.mask = LVIF_GROUPID;
		ListView_GetItem(playerList, &item);
		update_group(item.iGroupId);
		item.iGroupId = groupId;
		ListView_SetItem(playerList, &item);
	}

	update_group(groupId);

	item.mask = LVIF_STATE | LVIF_IMAGE;
	item.iSubItem = COLUMN_STATUS;
	item.stateMask = LVIS_OVERLAYMASK;

	if (!(battle_status & BS_AI)) {
		item.iImage = &s->user == g_my_battle->founder ? (battle_status & BS_MODE ? ICONS_HOST : ICONS_HOST_SPECTATOR)
			: s->user.client_status & CS_INGAME ? ICONS_INGAME
			: !(battle_status & BS_MODE) ? ICONS_SPECTATOR
			: battle_status & BS_READY ? ICONS_READY
			: ICONS_UNREADY;
		int iconIndex = USER_MASK;
		if (!(battle_status & SYNCED))
			iconIndex |= UNBS_SYNC;
		if (s->user.client_status & CS_AWAY)
			iconIndex |= AWAY_MASK;
		if (s->user.ignore)
			iconIndex |= IGNORE_MASK;
		item.state = INDEXTOOVERLAYMASK(iconIndex);
	}
	ListView_SetItem(playerList, &item);


#define setIcon(subItem, icon) \
	item.iSubItem = subItem; \
	item.iImage = icon; \
	ListView_SetItem(playerList, &item);

	int sideIcon = -1;
	if (battle_status & BS_MODE && *g_side_names[FROM_BS_SIDE(battle_status)])
		sideIcon = ICONS_FIRST_SIDE + FROM_BS_SIDE(battle_status);
	item.mask = LVIF_IMAGE;
	setIcon(COLUMN_SIDE, sideIcon);


	extern int IconList_get_user_color(const union UserOrBot *);
	setIcon(COLUMN_COLOR, IconList_get_user_color((void *)s));

	if (battle_status & BS_AI) {
		wchar_t name[MAX_NAME_LENGTH * 2 + 4];
		_swprintf(name, L"%hs (%hs)", s->name, s->bot.dll);
		item.mask = LVIF_TEXT;
		item.iSubItem = COLUMN_NAME;
		item.pszText = name;
		ListView_SetItem(playerList, &item);
		goto sort;
	}

	User *u = &s->user;


	assert(item.mask = LVIF_IMAGE);
	setIcon(COLUMN_FLAG, ICONS_FIRST_FLAG + u->country);
	setIcon(COLUMN_RANK, ICONS_FIRST_RANK + FROM_RANK_MASK(u->client_status));

	if (u == &g_my_user) {
		SendDlgItemMessage(g_battle_room, DLG_SPECTATE, BM_SETCHECK,
				!(battle_status & BS_MODE), 0);

		EnableWindow(GetDlgItem(g_battle_room, DLG_AUTO_UNSPEC),
				!(battle_status & BS_MODE));

		for (int i=0; i<=NUM_SIDE_BUTTONS; ++i)
			SendDlgItemMessage(g_battle_room, DLG_SIDE_FIRST + i, BM_SETCHECK,
					FROM_BS_SIDE(battle_status) == i, 0);

		SendDlgItemMessage(g_battle_room, DLG_ALLY, CB_SETCURSEL,
				FROM_BS_ALLY(battle_status), 0);
	}

	if (u->battle->founder == u) {
		HWND startButton = GetDlgItem(g_battle_room, DLG_START);
		int canJoin = !(g_my_user.client_status & CS_INGAME);
		EnableWindow(startButton, canJoin);
	}

sort:;
	int teamSizes[16] = {};

	FOR_EACH_PLAYER(u, g_my_battle)
		++teamSizes[FROM_BS_TEAM(u->battle_status)];

	FOR_EACH_USER(u, g_my_battle) {
		wchar_t buf[128], *s=buf;
		if ((u->battle_status & BS_MODE) && teamSizes[FROM_BS_TEAM(u->battle_status)] > 1)
			s += _swprintf(s, L"%d: ", FROM_BS_TEAM(u->battle_status)+1);
		s += _swprintf(s, L"%hs", u->name);
		if (strcmp(UNTAGGED_NAME(u->name), u->alias))
			s += _swprintf(s, L" (%hs)", u->alias);

		item.mask = LVIF_TEXT;
		item.iItem = find_user(u);
		item.iSubItem = COLUMN_NAME;
		item.pszText = buf;

		ListView_SetItem(playerList, &item);
	}
	SendMessage(playerList, LVM_SORTITEMS, 0, (LPARAM)sort_listview);
}

void
BattleRoom_resize_columns(void)
{
	HWND playerList = GetDlgItem(g_battle_room, DLG_PLAYERLIST);
	RECT rect;
	GetClientRect(playerList, &rect);
	for (int i=0; i <= COLUMN_LAST; ++i) {
		if (i != COLUMN_NAME) {
			ListView_SetColumnWidth(playerList, i, 24);
			rect.right -= ListView_GetColumnWidth(playerList, i);
		}
	}
	ListView_SetColumnWidth(playerList, COLUMN_NAME, rect.right);
}

static void
resize_all(LPARAM lParam)
{
#define INFO_WIDTH (MAP_Y(140))
#define LIST_WIDTH 280
#define INFO_HEIGHT (MAP_Y(200))

#define CHAT_WIDTH (width - MAP_X(2*50 + 7 + 4))
#define CHAT_TOP (INFO_HEIGHT + 2*YS)

#define S 2
#define XS MAP_X(S)
#define YS MAP_Y(S)
#define YH MAP_Y(14)

	int width = LOWORD(lParam), h = HIWORD(lParam);


	HDWP dwp = BeginDeferWindowPos(DLG_LAST + 1);

#define MOVE_ID(id, x, y, cx, cy)\
	(DeferWindowPos(dwp, (GetDlgItem(g_battle_room, (id))), NULL, (x), (y), (cx), (cy), 0))


	MOVE_ID(DLG_INFOLIST, XS, YS, INFO_WIDTH, INFO_HEIGHT);
	MOVE_ID(DLG_PLAYERLIST, INFO_WIDTH + 2*XS, YS, LIST_WIDTH, INFO_HEIGHT);
	int minimapX = INFO_WIDTH + LIST_WIDTH + 3*XS;
	MOVE_ID(DLG_MINIMAP, minimapX, MAP_Y(14 + 2*S), width - minimapX - XS, INFO_HEIGHT - MAP_Y(28 + 4*S));
	MOVE_ID(DLG_CHAT, XS, CHAT_TOP, CHAT_WIDTH - MAP_X(7), h - INFO_HEIGHT - 3*YS);

	MOVE_ID(DLG_START, CHAT_WIDTH, h - MAP_Y(14 + S), MAP_X(50), MAP_Y(14));
	MOVE_ID(DLG_LEAVE, CHAT_WIDTH + MAP_X(54), h - MAP_Y(14 + S), MAP_X(50), MAP_Y(14));

	MOVE_ID(DLG_ALLY_LABEL,  CHAT_WIDTH, CHAT_TOP, MAP_X(70), TEXTLABEL_Y);
	MOVE_ID(DLG_ALLY,        CHAT_WIDTH, CHAT_TOP + MAP_Y(10), MAP_X(70), COMMANDBUTTON_Y);

	MOVE_ID(DLG_SIDE_LABEL,  CHAT_WIDTH, CHAT_TOP + MAP_Y(31), MAP_X(70), TEXTLABEL_Y);

	for (int i=0; i <= DLG_SIDE_LAST - DLG_SIDE_FIRST; ++i)
		MOVE_ID(DLG_SIDE_FIRST + i, CHAT_WIDTH + i * (COMMANDBUTTON_Y + MAP_X(4)), CHAT_TOP + MAP_Y(41), COMMANDBUTTON_Y, COMMANDBUTTON_Y);

	MOVE_ID(DLG_SPECTATE,    CHAT_WIDTH, CHAT_TOP + MAP_Y(62), MAP_X(70), TEXTBOX_Y);
	MOVE_ID(DLG_AUTO_UNSPEC, CHAT_WIDTH + MAP_X(10), CHAT_TOP + MAP_Y(77), MAP_X(80), TEXTBOX_Y);

	for (int i=0; i<=SPLIT_LAST; ++i)
		MOVE_ID(DLG_SPLIT_FIRST + i, minimapX + (1 + i) * XS + i * YH, YS, MAP_Y(14), MAP_Y(14));
	MOVE_ID(DLG_SPLIT_SIZE, minimapX + 6*XS + 5*YH, YS, width - minimapX - 7*XS - 5*YH, YH);

#define TOP (INFO_HEIGHT - MAP_Y(14 + S))
	MOVE_ID(DLG_MAPMODE_MINIMAP,   minimapX + XS, TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_MAPMODE_METAL,     minimapX + XS + MAP_X(50),  TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_MAPMODE_ELEVATION, minimapX + XS + MAP_X(100), TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_CHANGE_MAP,        width - 2*XS - MAP_X(60),   TOP, MAP_X(60), COMMANDBUTTON_Y);
#undef TOP

	EndDeferWindowPos(dwp);
	BattleRoom_resize_columns();
	BattleRoom_redraw_minimap();
}

static RECT boundingRect;

static LRESULT CALLBACK
tooltip_subclass(HWND window, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (msg == WM_MOUSEMOVE
			&& (GET_X_LPARAM(lParam) < boundingRect.left
				|| GET_X_LPARAM(lParam) > boundingRect.right
				|| GET_Y_LPARAM(lParam) < boundingRect.top
				|| GET_Y_LPARAM(lParam) > boundingRect.bottom))
		SendMessage((HWND)dwRefData, TTM_POP, 0, 0);
	return DefSubclassProc(window, msg, wParam, lParam);
}

static void
on_create(HWND window)
{
	g_battle_room = window;
	CreateDlgItems(window, dialog_items, DLG_LAST + 1);
	for (int i=0; i<16; ++i) {
		wchar_t buf[LENGTH("Team 16")];
		_swprintf(buf, L"Team %d", i+1);
		SendDlgItemMessage(window, DLG_ALLY, CB_ADDSTRING, 0, (LPARAM)buf);
	}

	HWND infoList = GetDlgItem(window, DLG_INFOLIST);
#define INSERT_COLUMN(__w, __n) { \
	LVCOLUMN c; c.mask = 0; \
	ListView_InsertColumn((__w), (__n), &c);} \

	INSERT_COLUMN(infoList, 0);
	INSERT_COLUMN(infoList, 1);
	ListView_EnableGroupView(infoList, TRUE);
	ListView_SetExtendedListViewStyle(infoList, LVS_EX_FULLROWSELECT);

	HWND playerList = GetDlgItem(window, DLG_PLAYERLIST);
	for (int i=0; i <= COLUMN_LAST; ++i)
		INSERT_COLUMN(playerList, i);
#undef INSERT_COLUMN

	ListView_SetExtendedListViewStyle(playerList, LVS_EX_DOUBLEBUFFER |  LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
	SendMessage(playerList, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)g_icon_list);
	ListView_EnableGroupView(playerList, TRUE);
	for (int i=0; i<=16; ++i) {
		wchar_t buf[LENGTH("Spectators")];
		_swprintf(buf, i<16 ? L"Team %d" : L"Spectators", i+1);
		LVGROUP groupInfo;
		groupInfo.cbSize = sizeof(groupInfo);
		groupInfo.mask = LVGF_HEADER | LVGF_GROUPID;
		groupInfo.pszHeader = buf;
		groupInfo.iGroupId = i;
		ListView_InsertGroup(playerList, -1, &groupInfo);
	}

	HWND hwndTip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL, 0,
			0, 0, 0, 0,
			window, NULL, NULL, NULL);

	SendMessage(hwndTip, TTM_SETMAXTIPWIDTH, 0, 200);

	TOOLINFO toolInfo = {
		sizeof(toolInfo), TTF_SUBCLASS | TTF_IDISHWND | TTF_TRANSPARENT,
		window, (UINT_PTR)playerList,
		.lpszText = LPSTR_TEXTCALLBACK,
	};

	SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	SetWindowSubclass(playerList, tooltip_subclass, 0, (DWORD_PTR)hwndTip);

	toolInfo.uId = (UINT_PTR)infoList;
	SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	SetWindowSubclass(infoList, tooltip_subclass, 0, (DWORD_PTR)hwndTip);

	SendDlgItemMessage(window, DLG_SPLIT_SIZE, TBM_SETRANGE, 1,
			MAKELONG(0, 200));
	SendDlgItemMessage(window, DLG_SPLIT_FIRST + SPLIT_VERT,
			BM_SETIMAGE, IMAGE_ICON,
			(WPARAM)ImageList_GetIcon(g_icon_list, ICONS_SPLIT_VERT, 0));
	SendDlgItemMessage(window, DLG_SPLIT_FIRST + SPLIT_HORZ,
			BM_SETIMAGE, IMAGE_ICON,
			(WPARAM)ImageList_GetIcon(g_icon_list, ICONS_SPLIT_HORZ, 0));
	SendDlgItemMessage(window, DLG_SPLIT_FIRST + SPLIT_CORNERS1,
			BM_SETIMAGE, IMAGE_ICON,
			(WPARAM)ImageList_GetIcon(g_icon_list, ICONS_SPLIT_CORNER1, 0));
	SendDlgItemMessage(window, DLG_SPLIT_FIRST + SPLIT_CORNERS2,
			BM_SETIMAGE, IMAGE_ICON,
			(WPARAM)ImageList_GetIcon(g_icon_list, ICONS_SPLIT_CORNER2, 0));
	SendDlgItemMessage(window, DLG_SPLIT_FIRST + SPLIT_RAND,
			BM_SETIMAGE, IMAGE_ICON,
			(WPARAM)ImageList_GetIcon(g_icon_list, ICONS_SPLIT_RAND, 0));

	SendDlgItemMessage(window, DLG_MAPMODE_MINIMAP, BM_SETCHECK, BST_CHECKED, 0);
}

static void
on_draw(DRAWITEMSTRUCT *info)
{
	FillRect(info->hDC, &info->rcItem, (HBRUSH) (COLOR_BTNFACE+1));

	if (!minimapPixels) {
		/* extern void
GetDownloadMessage(const char *text); */
		/* char text[256]; */
		/* GetDownloadMessage(text); */
		/* DrawTextA(info->hDC, text, -1, &info->rcItem, DT_CENTER); */
		return;
	}

	int width = info->rcItem.right;
	int height = info->rcItem.bottom;

	if (!g_map_info.width || !g_map_info.height || !width || !height)
		return;

	if (height * g_map_info.width > width * g_map_info.height)
		height = width * g_map_info.height / g_map_info.width;
	else
		width = height * g_map_info.width / g_map_info.height;

	int xOffset = (info->rcItem.right - width) / 2;
	int yOffset = (info->rcItem.bottom - height) / 2;


	uint32_t *pixels = malloc(width * height * 4);
	if (SendDlgItemMessage(g_battle_room, DLG_MAPMODE_ELEVATION, BM_GETCHECK, 0, 0)) {
		for (int i=0; i<width * height; ++i) {
			uint8_t heightPixel = heightMapPixels[i % width * heightMapWidth / width + i / width * heightMapHeight / height * heightMapWidth];
			pixels[i] = heightPixel | heightPixel << 8 | heightPixel << 16;
		}
	} else if (SendDlgItemMessage(g_battle_room, DLG_MAPMODE_METAL, BM_GETCHECK, 0, 0)) {
		for (int i=0; i<width * height; ++i) {
			uint16_t p = minimapPixels[i % width * MAP_RESOLUTION / width + i / width * MAP_RESOLUTION / height * MAP_RESOLUTION];
			uint8_t metalPixel = metalMapPixels[i % width * metalMapWidth / width + i / width * metalMapHeight / height * metalMapWidth];
			pixels[i] = (p & 0x001B) << 1 | (p & 0x700 ) << 3 | (p & 0xE000) << 6 | metalPixel >> 2 | metalPixel << 8;
		}
	} else {
		for (int i=0; i<width * height; ++i) {
			uint16_t p = minimapPixels[i % width * MAP_RESOLUTION / width + i / width * MAP_RESOLUTION / height * MAP_RESOLUTION];
			pixels[i] = (p & 0x001F) << 3 | (p & 0x7E0 ) << 5 | (p & 0xF800) << 8;
		}
	}
	if (!g_my_battle)
		goto cleanup;

	if (g_battle_options.startPosType == STARTPOS_CHOOSE_INGAME) {
		for (int j=0; j<NUM_ALLIANCES; ++j) {
			int xMin = g_battle_options.startRects[j].left * width / START_RECT_MAX;
			int xMax = g_battle_options.startRects[j].right * width / START_RECT_MAX;
			int yMin = g_battle_options.startRects[j].top * height / START_RECT_MAX;
			int yMax = g_battle_options.startRects[j].bottom * height / START_RECT_MAX;

			if ((g_my_user.battle_status & BS_MODE) && j == FROM_BS_ALLY(g_my_user.battle_status)) {
				for (int x=xMin; x<xMax; ++x) {
					for (int y=yMin; y<yMax; ++y) {
						if ((pixels[x+width*y] & 0x00FF00) >= (0x00FF00 - 0x003000))
							pixels[x+width*y] |= 0x00FF00;
						else
							pixels[x+width*y] += 0x003000;
					}
				}
			} else {
				for (int x=xMin; x<xMax; ++x) {
					for (int y=yMin; y<yMax; ++y) {
						if ((pixels[x+width*y] & 0xFF0000) >= (0xFF0000 - 0x300000))
							pixels[x+width*y] |= 0xFF0000;
						else
							pixels[x+width*y] += 0x300000;
					}
				}
			}
			for (int x=xMin; x<xMax; ++x) {
				pixels[x+width*yMin] = 0;
				pixels[x+width*(yMax-1)] = 0;
			}
			for (int y=yMin; y<yMax; ++y) {
				pixels[xMin+width*y] = 0;
				pixels[(xMax-1)+width*y] = 0;
			}
		}

	} else {
		int max = g_my_battle ? GetNumPlayers(g_my_battle) : 0;
		max = max < g_map_info.posCount ? max : g_map_info.posCount;
		for (int i=0; i<max; ++i) {
			int xMid = g_map_info.positions[i].x * width / g_map_info.width;
			int yMid = g_map_info.positions[i].z * height / g_map_info.height;

			for (int x=xMid-5; x<xMid+5; ++x)
				for (int y=yMid-5; y<yMid+5; ++y)
					pixels[x + width * y] = 0x00CC00;
		}
	}

cleanup:;
	HBITMAP bitmap = CreateBitmap(width, height, 1, 32, pixels);
	free(pixels);

	HDC dcSrc = CreateCompatibleDC(info->hDC);
	SelectObject(dcSrc, bitmap);
	BitBlt(info->hDC, xOffset, yOffset, width, height, dcSrc, 0, 0, SRCCOPY);

	DeleteObject(bitmap);
}

static wchar_t *
get_tooltip(const User *u)
{
	static wchar_t buf[128];
	int buff_used = 0;

#define APPEND(...) { \
	int read = _snwprintf(buf + buff_used, \
			sizeof(buf) / sizeof(*buf) - buff_used - 1, \
			__VA_ARGS__); \
	\
	if (read < 0) {\
		buf[sizeof(buf) / sizeof(*buf) - 1] = '\0'; \
		return buf; \
	} \
	buff_used += read; \
}

	APPEND(L"%hs", u->name);

	if (strcmp(UNTAGGED_NAME(u->name), u->alias))
		APPEND(L" (%hs)", u->alias);

	if (!(u->battle_status & BS_AI))
		APPEND(L"\nRank %d - %hs - %.2fGHz\n",
				FROM_RANK_MASK(u->client_status),
				Country_get_name(u->country),
				(float)u->cpu / 1000);

	if (!(u->battle_status & BS_MODE)) {
		APPEND(L"Spectator");
		return buf;
	}

	const char *sideName = g_side_names[FROM_BS_SIDE(u->battle_status)];

	APPEND(L"Player %d - Team %d",
			FROM_BS_TEAM(u->battle_status),
			FROM_BS_ALLY(u->battle_status));
	if (*sideName)
		APPEND(L" - %hs", sideName);

	if (u->skill)
		APPEND(L"\nSkill: %hs", u->skill);

	if (u->battle_status & BS_HANDICAP)
		APPEND(L"\nHandicap: %d",
				FROM_BS_HANDICAP(u->battle_status));
	return buf;
#undef APPEND
}

static LRESULT
on_notify(WPARAM wParam, NMHDR *note)
{
	LVITEM item = {LVIF_PARAM};
	LVHITTESTINFO hitTestInfo = {};

	switch (note->code) {

	case TTN_GETDISPINFO:
		GetCursorPos(&hitTestInfo.pt);
		ScreenToClient((HWND)note->idFrom, &hitTestInfo.pt);

		item.iItem = SendMessage((HWND)note->idFrom, LVM_HITTEST, 0,
				(LPARAM)&hitTestInfo);

		if (item.iItem == -1)
			return 0;

		boundingRect.left = LVIR_BOUNDS;
		SendMessage((HWND)note->idFrom, LVM_GETITEMRECT, item.iItem,
				(LPARAM)&boundingRect);

		SendMessage((HWND)note->idFrom, LVM_GETITEM, 0, (LPARAM)&item);
		if (!item.lParam)
			return 0;

		if (GetDlgCtrlID((HWND)note->idFrom) == DLG_PLAYERLIST) {

			((NMTTDISPINFO *)note)->lpszText = get_tooltip((User *)item.lParam);
			return 0;
		}
		((NMTTDISPINFO *)note)->lpszText = utf8to16(((Option *)item.lParam)->desc);
		return 0;

	case LVN_ITEMACTIVATE:
		item.iItem = ((LPNMITEMACTIVATE)note)->iItem;
		SendMessage(note->hwndFrom, LVM_GETITEM, 0, (LPARAM)&item);
		if (note->idFrom == DLG_INFOLIST)
			MyBattle_change_option((Option *)item.lParam);
		else if (note->idFrom == DLG_PLAYERLIST)
			ChatWindow_set_active_tab(Chat_get_private_window((User *)item.lParam));
		return 1;

	case NM_RCLICK:
		if (note->idFrom != DLG_PLAYERLIST)
			return 0;
		hitTestInfo.pt =  ((LPNMITEMACTIVATE)note)->ptAction;
		item.iItem = SendMessage(note->hwndFrom, LVM_SUBITEMHITTEST, 0,
				(LPARAM)&hitTestInfo);
		if (item.iItem < 0)
			return 0;

		SendMessage(note->hwndFrom, LVM_GETITEM, 0, (LPARAM)&item);
		UserMenu_spawn((UserOrBot *)item.lParam, g_battle_room);
		return 1;

		break;
	}
	return DefWindowProc(g_battle_room, WM_NOTIFY, wParam, (LPARAM)note);
}

static LRESULT
on_command(WPARAM wParam, HWND window)
{
	HMENU menu;

	switch (wParam) {
	case MAKEWPARAM(DLG_START, BN_CLICKED):
		if (g_host_type && g_host_type->start_game)
			g_host_type->start_game();
		else
			Spring_launch();
		return 0;

	case MAKEWPARAM(DLG_CHANGE_MAP, BN_CLICKED):
		menu = CreatePopupMenu();
		for (int i=0; i<g_map_count; ++i)
			AppendMenuA(menu, MF_CHECKED * !strcmp(g_my_battle->map_name,  g_maps[i]), i + 1, g_maps[i]);
		POINT pt;
		GetCursorPos(&pt);
		int mapIndex = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y, g_battle_room, NULL);
		if (mapIndex > 0)
			ChangeMap(g_maps[mapIndex - 1]);
		DestroyMenu(menu);
		return 0;

		//TODO: This seems wrong to me. Too lazy to fix.
	case MAKEWPARAM(DLG_SPLIT_FIRST, BN_CLICKED) ... MAKEWPARAM(DLG_SPLIT_LAST, BN_CLICKED):
		MyBattle_set_split(LOWORD(wParam) - DLG_SPLIT_FIRST, SendDlgItemMessage(g_battle_room, DLG_SPLIT_SIZE, TBM_GETPOS, 0, 0));
		return 0;

	case MAKEWPARAM(DLG_LEAVE, BN_CLICKED):
		SendMessage(g_battle_room, WM_CLOSE, 0, 0);
		return 0;

	case MAKEWPARAM(DLG_SPECTATE, BN_CLICKED):
		if (g_my_user.battle_status & BS_MODE)
			SendDlgItemMessage(g_battle_room, DLG_AUTO_UNSPEC,
					BM_SETCHECK, BST_UNCHECKED, 0);

		SetBattleStatus(&g_my_user, ~g_my_user.battle_status, BS_MODE);
		return 0;

	case MAKEWPARAM(DLG_ALLY, CBN_SELCHANGE):
		SetBattleStatus(&g_my_user, TO_BS_ALLY_MASK(SendMessage(window, CB_GETCURSEL, 0, 0)), BS_ALLY);
		SendMessage(window, CB_SETCURSEL, FROM_BS_ALLY(g_my_user.battle_status), 0);
		return 0;

	case MAKEWPARAM(DLG_SIDE_FIRST, BN_CLICKED) ... MAKEWPARAM(DLG_SIDE_LAST, BN_CLICKED):
		SetBattleStatus(&g_my_user, TO_BS_SIDE_MASK(LOWORD(wParam) - DLG_SIDE_FIRST), BS_SIDE);
		return 0;

	case MAKEWPARAM(DLG_MAPMODE_MINIMAP, BN_CLICKED) ... MAKEWPARAM(DLG_MAPMODE_ELEVATION, BN_CLICKED):
		InvalidateRect(GetDlgItem(g_battle_room, DLG_MINIMAP), 0, 0);
		return 0;
	}
	return 1;
}

static void
set_details(const Option *options, ssize_t nbOptions)
{
	HWND infoList = GetDlgItem(g_battle_room, DLG_INFOLIST);

	LVGROUP group;
	group.cbSize = sizeof(group);
	group.mask = LVGF_HEADER | LVGF_GROUPID;

	for (ssize_t i=0; i<nbOptions; ++i) {
		if (options[i].type != opt_section)
			continue;
		group.pszHeader = utf8to16(options[i].name);
		group.iGroupId = (intptr_t)&options[i];
		SendMessage(infoList, LVM_INSERTGROUP, -1, (LPARAM)&group);
	}

	LVITEMA item;
	item.mask = LVIF_GROUPID | LVIF_TEXT | LVIF_PARAM;
	item.iItem = INT_MAX;
	item.iSubItem = 0;

	for (ssize_t i=0; i<nbOptions; ++i) {
		if (options[i].type == opt_section)
			continue;

		item.pszText = options[i].name;
		item.lParam = (LPARAM)&options[i];
		item.iGroupId = (intptr_t)options[i].section;

		SendMessageA(infoList, LVM_INSERTITEMA, 0, (LPARAM)&item);
	}
}

/* static void
set_mapInfo(void) */
/* { */
/* HWND infoList = GetDlgItem(g_battle_room, DLG_INFOLIST); */
/* LVGROUP group; */
/* group.cbSize = sizeof(group); */
/* group.mask = LVGF_HEADER | LVGF_GROUPID; */
/* group.pszHeader = L"Map Info"; */
/* group.iGroupId = 1; */
/* SendMessage(infoList, LVM_INSERTGROUP, -1, (LPARAM)&group); */

/* LVITEM item; */

/* #define ADD_STR(s1, s2) \ */
/* item.mask = LVIF_TEXT | LVIF_GROUPID; \ */
/* item.iItem = INT_MAX; \ */
/* item.iSubItem = 0; \ */
/* item.pszText = s1; \ */
/* item.iGroupId = 1; \ */
/* \ */
/* item.iItem = SendMessage(infoList, LVM_INSERTITEM, 0, (LPARAM)&item); \ */
/* item.mask = LVIF_TEXT; \ */
/* item.iSubItem = 1; \ */
/* item.pszText = s2; \ */
/* SendMessage(infoList, LVM_SETITEM, 0, (LPARAM)&item); */

/* if (g_map_info.author[0]) */
/* ADD_STR(L"Author", utf8to16(g_map_info.author)); */
/* ADD_STR(L"Tidal: ", utf8to16(g_map_info.author)); */

/* #undef ADD_STR */
/* } */

void
BattleRoom_on_set_mod_details(void)
{
	HWND infoList = GetDlgItem(g_battle_room, DLG_INFOLIST);
	SendMessage(infoList, LVM_REMOVEALLGROUPS, 0, 0);
	ListView_DeleteAllItems(infoList);

	set_details(g_mod_options, g_mod_option_count);
	set_details(g_map_options, g_map_option_count);
	/* set_mapInfo(); */

	ListView_SetColumnWidth(infoList, 0, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(infoList, 1, LVSCW_AUTOSIZE_USEHEADER);
}

void
BattleRoom_on_set_option(Option *opt)
{
	HWND infoList = GetDlgItem(g_battle_room, DLG_INFOLIST);

	LVFINDINFO find_info;
	find_info.flags = LVFI_PARAM;
	find_info.lParam = (LPARAM)opt;

	LVITEMA item;
	item.mask = LVIF_TEXT;
	item.iSubItem = 1;
	item.pszText = opt->val ?: opt->def;
	item.iItem = ListView_FindItem(infoList, -1, &find_info);

	SendMessageA(infoList, LVM_SETITEMA, 0, (LPARAM)&item);
}

void
BattleRoom_on_change_mod(void)
{
	for (int i=0; i<=DLG_SIDE_LAST - DLG_SIDE_FIRST; ++i) {
		HWND sideButton = GetDlgItem(g_battle_room, DLG_SIDE_FIRST + i);
		SendMessage(sideButton, BM_SETIMAGE, IMAGE_ICON, (WPARAM)ImageList_GetIcon(g_icon_list, ICONS_FIRST_SIDE + i, 0));
		ShowWindow(sideButton, i < g_side_count);
	}
	HWND playerList = GetDlgItem(g_battle_room, DLG_PLAYERLIST);
	LVITEM item = {LVIF_PARAM};
	for (item.iItem= -1; (item.iItem = SendMessage(playerList, LVM_GETNEXTITEM, item.iItem, 0)) >= 0;) {
		item.mask = LVIF_PARAM;
		SendMessage(playerList, LVM_GETITEM, 0, (LPARAM)&item);
		item.mask = LVIF_IMAGE;
		item.iSubItem = COLUMN_SIDE;
		union UserOrBot *s = (void *)item.lParam;
		item.iImage = s->battle_status & BS_MODE && *g_side_names[FROM_BS_SIDE(s->battle_status)] ? ICONS_FIRST_SIDE + FROM_BS_SIDE(s->battle_status) : -1;
		SendMessage(playerList, LVM_SETITEM, 0, (LPARAM)&item);
	}
}

static LRESULT CALLBACK
battle_room_proc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CREATE:
		on_create(window);
		return 0;
	case WM_SIZE:
		resize_all(lParam);
		break;
	case WM_DRAWITEM:
		if (wParam == DLG_MINIMAP)
			on_draw((void *)lParam);
		return 1;
	case WM_NOTIFY:
		return on_notify(wParam, (NMHDR *)lParam);
	case WM_DESTROY:
		g_battle_room = NULL;
		break;
	case WM_HSCROLL:
		if (GetDlgCtrlID((HWND)lParam) != DLG_SPLIT_SIZE
				|| wParam != SB_ENDSCROLL)
			return 0;

		for (int i=0; i<=SPLIT_LAST; ++i)
			if (SendDlgItemMessage(g_battle_room, DLG_SPLIT_FIRST + i, BM_GETCHECK, 0, 0))
				MyBattle_set_split(i, SendDlgItemMessage(window, DLG_SPLIT_SIZE, TBM_GETPOS, 0, 0));
		return 0;
	case WM_CLOSE:
		MainWindow_disable_battleroom_button();
		LeaveBattle();
		return 0;
	case WM_COMMAND:
		return on_command(wParam, (HWND)lParam);
	}
	return DefWindowProc(window, msg, wParam, lParam);
}

static void __attribute__((constructor))
init (void)
{
	RegisterClassEx((&(WNDCLASSEX){
				.lpszClassName = WC_BATTLEROOM,
				.cbSize        = sizeof(WNDCLASSEX),
				.lpfnWndProc   = battle_room_proc,
				.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
				.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
				}));
}

