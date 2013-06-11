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

#define NUM_SIDE_BUTTONS 4

enum PlayerListColumn {
	COLUMN_STATUS,
	COLUMN_RANK,
	COLUMN_NAME,
	COLUMN_COLOR,
	COLUMN_SIDE,
	COLUMN_FLAG,
	COLUMN_LAST = COLUMN_FLAG,
};

enum DialogId {
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

static const uint16_t *minimap_pixels;
static uint16_t metal_map_height, metal_map_width;
static const uint8_t *metal_map_pixels;
static uint16_t height_map_height, height_map_width;
static const uint8_t *height_map_pixels;
static RECT bounding_rect;

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
		.ex_style = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | LVS_SHAREIMAGELISTS | LVS_SINGLESEL | LVS_REPORT | LVS_NOCOLUMNHEADER,
	}, [DLG_INFOLIST] = {
		.class = WC_LISTVIEW,
		.ex_style = WS_EX_CLIENTEDGE,
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
BattleRoom_change_minimap_bitmap(const uint16_t *_minimap_pixels,
		uint16_t _metal_mapWidth, uint16_t _metal_mapHeight, const uint8_t *_metal_mapPixels,
		uint16_t _height_mapWidth, uint16_t _height_mapHeight, const uint8_t *_height_mapPixels)
{
	minimap_pixels = _minimap_pixels;

	metal_map_width = _metal_mapWidth;
	metal_map_height = _metal_mapHeight;
	metal_map_pixels = _metal_mapPixels;

	height_map_width = _height_mapWidth;
	height_map_height = _height_mapHeight;
	height_map_pixels = _height_mapPixels;

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
	for (SplitType i=0; i<=SPLIT_LAST; ++i)
		SendDlgItemMessage(g_battle_room, DLG_SPLIT_FIRST + i,
				BM_SETCHECK, i == type, 0);

	EnableWindow(GetDlgItem(g_battle_room, DLG_SPLIT_SIZE),
			g_battle_options.start_pos_type == STARTPOS_CHOOSE_INGAME);

	SendDlgItemMessage(g_battle_room, DLG_SPLIT_SIZE, TBM_SETPOS, 1, size);
}

void
BattleRoom_on_start_position_change(void)
{
	StartRect r1, r2;

	if (!g_battle_info_finished)
		return;

	BattleRoom_redraw_minimap();

	if (g_battle_options.start_pos_type == STARTPOS_RANDOM) {
		set_split(SPLIT_RAND, 0);
		return;
	}

	if (g_battle_options.start_pos_type != STARTPOS_CHOOSE_INGAME) {
		set_split(SPLIT_LAST + 1, 0);
		return;
	}

	if (g_battle_options.start_rects[0].left == 0
			&& (g_battle_options.start_rects[0].top == 0
				|| g_battle_options.start_rects[1].left != 0)) {
		r1 = g_battle_options.start_rects[0];
		r2 = g_battle_options.start_rects[1];
	} else {
		r1 = g_battle_options.start_rects[1];
		r2 = g_battle_options.start_rects[0];
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
sort_listview(const union UserOrBot *u1, const union UserOrBot *u2,
		__attribute__((unused)) LPARAM unused)
{
	return _stricmp(get_effective_name(u1), get_effective_name(u2));
}

static void
update_group(uint8_t group_id)
{
	wchar_t buf[64];
	uint8_t players_on_team;
	LVGROUP group_info;

	if (group_id >= 16)
		return;

	players_on_team = 0;
	FOR_EACH_PLAYER(p, g_my_battle)
		players_on_team += FROM_BS_ALLY(p->battle_status) == group_id;
	_swprintf(buf, L"Team %d :: %hu Player%c", group_id + 1, players_on_team, players_on_team > 1 ? 's' : '\0');

	group_info.cbSize = sizeof(group_info);
	group_info.mask = LVGF_HEADER;
	group_info.pszHeader = buf;
	SendDlgItemMessage(g_battle_room, DLG_PLAYERLIST, LVM_SETGROUPINFO,
			group_id, (LPARAM)&group_info);
}

bool
BattleRoom_is_auto_unspec(void)
{
	return SendDlgItemMessage(g_battle_room, DLG_AUTO_UNSPEC, BM_GETCHECK, 0, 0);
}

void
BattleRoom_update_user(union UserOrBot *s)
{
	HWND player_list = GetDlgItem(g_battle_room, DLG_PLAYERLIST);

	uint32_t battle_status = s->battle_status;
	uint8_t group_id = (battle_status & BS_MODE) ? FROM_BS_ALLY(battle_status) : 16;

	LVITEM item;
	item.iItem = find_user(s);
	item.iSubItem = 0;

	if (item.iItem == -1) {
		item.mask = LVIF_PARAM | LVIF_GROUPID,
		item.iItem = 0;
		item.lParam = (LPARAM)s;
		item.iGroupId = group_id;
		ListView_InsertItem(player_list, &item);

	} else {
		item.mask = LVIF_GROUPID;
		ListView_GetItem(player_list, &item);
		update_group(item.iGroupId);
		item.iGroupId = group_id;
		ListView_SetItem(player_list, &item);
	}

	update_group(group_id);

	item.mask = LVIF_STATE | LVIF_IMAGE;
	item.iSubItem = COLUMN_STATUS;
	item.stateMask = LVIS_OVERLAYMASK;

	if (!(battle_status & BS_AI)) {
		item.iImage = &s->user == g_my_battle->founder ? (battle_status & BS_MODE ? ICON_HOST : ICON_HOST_SPECTATOR)
			: s->user.client_status & CS_INGAME ? ICON_INGAME
			: !(battle_status & BS_MODE) ? ICON_SPECTATOR
			: battle_status & BS_READY ? ICON_READY
			: ICON_UNREADY;
		int icon_index = USER_MASK;
		if (!(battle_status & SYNCED))
			icon_index |= UNBS_SYNC;
		if (s->user.client_status & CS_AWAY)
			icon_index |= AWAY_MASK;
		if (s->user.ignore)
			icon_index |= IGNORE_MASK;
		item.state = INDEXTOOVERLAYMASK(icon_index);
	}
	ListView_SetItem(player_list, &item);


#define set_icon(sub_item, icon) \
	item.iSubItem = sub_item; \
	item.iImage = icon; \
	ListView_SetItem(player_list, &item);

	int side_icon = -1;
	if (battle_status & BS_MODE && *g_side_names[FROM_BS_SIDE(battle_status)])
		side_icon = ICON_FIRST_SIDE + FROM_BS_SIDE(battle_status);
	item.mask = LVIF_IMAGE;
	set_icon(COLUMN_SIDE, side_icon);


	extern int IconList_get_user_color(const union UserOrBot *);
	set_icon(COLUMN_COLOR, IconList_get_user_color((void *)s));

	if (battle_status & BS_AI) {
		wchar_t name[MAX_NAME_LENGTH * 2 + 4];
		_swprintf(name, L"%hs (%hs)", s->name, s->bot.dll);
		item.mask = LVIF_TEXT;
		item.iSubItem = COLUMN_NAME;
		item.pszText = name;
		ListView_SetItem(player_list, &item);
		goto sort;
	}

	User *u = &s->user;


	assert(item.mask = LVIF_IMAGE);
	set_icon(COLUMN_FLAG, ICON_FIRST_FLAG + u->country);
	set_icon(COLUMN_RANK, ICON_FIRST_RANK + FROM_RANK_MASK(u->client_status));

	if (u == &g_my_user) {
		SendDlgItemMessage(g_battle_room, DLG_SPECTATE, BM_SETCHECK,
				!(battle_status & BS_MODE), 0);

		EnableWindow(GetDlgItem(g_battle_room, DLG_AUTO_UNSPEC),
				!(battle_status & BS_MODE));

		for (size_t i=0; i<=NUM_SIDE_BUTTONS; ++i)
			SendDlgItemMessage(g_battle_room, DLG_SIDE_FIRST + i, BM_SETCHECK,
					FROM_BS_SIDE(battle_status) == i, 0);

		SendDlgItemMessage(g_battle_room, DLG_ALLY, CB_SETCURSEL,
				FROM_BS_ALLY(battle_status), 0);
	}

	if (u->battle->founder == u) {
		HWND start_button = GetDlgItem(g_battle_room, DLG_START);
		int can_join = !(g_my_user.client_status & CS_INGAME);
		EnableWindow(start_button, can_join);
	}

sort:;
	int team_sizes[16] = {};

	FOR_EACH_PLAYER(u, g_my_battle)
		++team_sizes[FROM_BS_TEAM(u->battle_status)];

	FOR_EACH_USER(u, g_my_battle) {
		wchar_t buf[128], *s=buf;
		if ((u->battle_status & BS_MODE) && team_sizes[FROM_BS_TEAM(u->battle_status)] > 1)
			s += _swprintf(s, L"%d: ", FROM_BS_TEAM(u->battle_status)+1);
		s += _swprintf(s, L"%hs", u->name);
		if (strcmp(UNTAGGED_NAME(u->name), u->alias))
			s += _swprintf(s, L" (%hs)", u->alias);

		item.mask = LVIF_TEXT;
		item.iItem = find_user(u);
		item.iSubItem = COLUMN_NAME;
		item.pszText = buf;

		ListView_SetItem(player_list, &item);
	}
	SendMessage(player_list, LVM_SORTITEMS, 0, (LPARAM)sort_listview);
}

void
BattleRoom_resize_columns(void)
{
	RECT rect;
	HWND player_list;
	int total_width = 0;

	player_list = GetDlgItem(g_battle_room, DLG_PLAYERLIST);

	for (enum PlayerListColumn i=0; i <= COLUMN_LAST; ++i) {
		if (i != COLUMN_NAME) {
			ListView_SetColumnWidth(player_list, i, 24);
			total_width += ListView_GetColumnWidth(player_list, i);
		}
	}

	GetClientRect(player_list, &rect);

	ListView_SetColumnWidth(player_list, COLUMN_NAME,
			rect.right - total_width);
}

static void
resize_all(LPARAM l_param)
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

	int width = LOWORD(l_param), h = HIWORD(l_param);


	HDWP dwp = BeginDeferWindowPos(DLG_LAST + 1);

#define MOVE_ID(id, x, y, cx, cy)\
	(DeferWindowPos(dwp, (GetDlgItem(g_battle_room, (id))), NULL, (x), (y), (cx), (cy), 0))


	MOVE_ID(DLG_INFOLIST, XS, YS, INFO_WIDTH, INFO_HEIGHT);
	MOVE_ID(DLG_PLAYERLIST, INFO_WIDTH + 2*XS, YS, LIST_WIDTH, INFO_HEIGHT);
	int minimap_x = INFO_WIDTH + LIST_WIDTH + 3*XS;
	MOVE_ID(DLG_MINIMAP, minimap_x, MAP_Y(14 + 2*S), width - minimap_x - XS, INFO_HEIGHT - MAP_Y(28 + 4*S));
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

	for (SplitType i=0; i<=SPLIT_LAST; ++i)
		MOVE_ID(DLG_SPLIT_FIRST + i, minimap_x + (1 + i) * XS + i * YH, YS, MAP_Y(14), MAP_Y(14));
	MOVE_ID(DLG_SPLIT_SIZE, minimap_x + 6*XS + 5*YH, YS, width - minimap_x - 7*XS - 5*YH, YH);

#define TOP (INFO_HEIGHT - MAP_Y(14 + S))
	MOVE_ID(DLG_MAPMODE_MINIMAP,   minimap_x + XS, TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_MAPMODE_METAL,     minimap_x + XS + MAP_X(50),  TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_MAPMODE_ELEVATION, minimap_x + XS + MAP_X(100), TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_CHANGE_MAP,        width - 2*XS - MAP_X(60),   TOP, MAP_X(60), COMMANDBUTTON_Y);
#undef TOP

	EndDeferWindowPos(dwp);
	BattleRoom_resize_columns();
	BattleRoom_redraw_minimap();
}

static LRESULT CALLBACK
tooltip_subclass(HWND window, UINT msg, WPARAM w_param, LPARAM l_param,
		__attribute__((unused)) UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (msg == WM_MOUSEMOVE
			&& (GET_X_LPARAM(l_param) < bounding_rect.left
				|| GET_X_LPARAM(l_param) > bounding_rect.right
				|| GET_Y_LPARAM(l_param) < bounding_rect.top
				|| GET_Y_LPARAM(l_param) > bounding_rect.bottom))
		SendMessage((HWND)dwRefData, TTM_POP, 0, 0);

	return DefSubclassProc(window, msg, w_param, l_param);
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

	HWND info_list = GetDlgItem(window, DLG_INFOLIST);
#define INSERT_COLUMN(__w, __n) { \
	LVCOLUMN c; c.mask = 0; \
	ListView_InsertColumn((__w), (__n), &c);} \

	INSERT_COLUMN(info_list, 0);
	INSERT_COLUMN(info_list, 1);
	ListView_EnableGroupView(info_list, TRUE);
	ListView_SetExtendedListViewStyle(info_list, LVS_EX_FULLROWSELECT);

	HWND player_list = GetDlgItem(window, DLG_PLAYERLIST);
	for (int i=0; i <= COLUMN_LAST; ++i)
		INSERT_COLUMN(player_list, i);
#undef INSERT_COLUMN

	ListView_SetExtendedListViewStyle(player_list, LVS_EX_DOUBLEBUFFER |  LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
	IconList_enable_for_listview(player_list);
	ListView_EnableGroupView(player_list, TRUE);
	for (int i=0; i<=16; ++i) {
		wchar_t buf[LENGTH("Spectators")];
		_swprintf(buf, i<16 ? L"Team %d" : L"Spectators", i+1);
		LVGROUP group_info;
		group_info.cbSize = sizeof(group_info);
		group_info.mask = LVGF_HEADER | LVGF_GROUPID;
		group_info.pszHeader = buf;
		group_info.iGroupId = i;
		ListView_InsertGroup(player_list, -1, &group_info);
	}

	HWND hwndTip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL, 0,
			0, 0, 0, 0,
			window, NULL, NULL, NULL);

	SendMessage(hwndTip, TTM_SETMAXTIPWIDTH, 0, 200);

	TOOLINFO tool_info = {
		sizeof(tool_info), TTF_SUBCLASS | TTF_IDISHWND | TTF_TRANSPARENT,
		window, (UINT_PTR)player_list,
		.lpszText = LPSTR_TEXTCALLBACK,
	};

	SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&tool_info);
	SetWindowSubclass(player_list, tooltip_subclass, 0, (DWORD_PTR)hwndTip);

	tool_info.uId = (UINT_PTR)info_list;
	SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&tool_info);
	SetWindowSubclass(info_list, tooltip_subclass, 0, (DWORD_PTR)hwndTip);

	SendDlgItemMessage(window, DLG_SPLIT_SIZE, TBM_SETRANGE, 1,
			MAKELONG(0, 200));

	IconList_set_window_image(GetDlgItem(window, DLG_SPLIT_FIRST + SPLIT_VERT),
			ICON_SPLIT_VERT);
	IconList_set_window_image(GetDlgItem(window, DLG_SPLIT_FIRST + SPLIT_HORZ),
			ICON_SPLIT_HORZ);
	IconList_set_window_image(GetDlgItem(window, DLG_SPLIT_FIRST + SPLIT_CORNERS1),
			ICON_SPLIT_CORNER1);
	IconList_set_window_image(GetDlgItem(window, DLG_SPLIT_FIRST + SPLIT_CORNERS2),
			ICON_SPLIT_CORNER2);
	IconList_set_window_image(GetDlgItem(window, DLG_SPLIT_FIRST + SPLIT_RAND),
			ICON_SPLIT_RAND);

	SendDlgItemMessage(window, DLG_MAPMODE_MINIMAP, BM_SETCHECK, BST_CHECKED, 0);
}

static void
on_draw(DRAWITEMSTRUCT *info)
{
	FillRect(info->hDC, &info->rcItem, (HBRUSH) (COLOR_BTNFACE+1));

	if (!minimap_pixels) {
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

	int x_offset = (info->rcItem.right - width) / 2;
	int y_offset = (info->rcItem.bottom - height) / 2;


	uint32_t *pixels = malloc(width * height * 4);
	if (SendDlgItemMessage(g_battle_room, DLG_MAPMODE_ELEVATION, BM_GETCHECK, 0, 0)) {
		for (int i=0; i<width * height; ++i) {
			uint8_t height_pixel = height_map_pixels[i % width * height_map_width / width + i / width * height_map_height / height * height_map_width];
			pixels[i] = height_pixel | height_pixel << 8 | height_pixel << 16;
		}
	} else if (SendDlgItemMessage(g_battle_room, DLG_MAPMODE_METAL, BM_GETCHECK, 0, 0)) {
		for (int i=0; i<width * height; ++i) {
			uint16_t p = minimap_pixels[i % width * MAP_RESOLUTION / width + i / width * MAP_RESOLUTION / height * MAP_RESOLUTION];
			uint8_t metal_pixel = metal_map_pixels[i % width * metal_map_width / width + i / width * metal_map_height / height * metal_map_width];
			pixels[i] = (p & 0x001B) << 1 | (p & 0x700 ) << 3 | (p & 0xE000) << 6 | metal_pixel >> 2 | metal_pixel << 8;
		}
	} else {
		for (int i=0; i<width * height; ++i) {
			uint16_t p = minimap_pixels[i % width * MAP_RESOLUTION / width + i / width * MAP_RESOLUTION / height * MAP_RESOLUTION];
			pixels[i] = (p & 0x001F) << 3 | (p & 0x7E0 ) << 5 | (p & 0xF800) << 8;
		}
	}
	if (!g_my_battle)
		goto cleanup;

	if (g_battle_options.start_pos_type == STARTPOS_CHOOSE_INGAME) {
		for (uint8_t j=0; j<NUM_ALLIANCES; ++j) {
			uint16_t x_min = g_battle_options.start_rects[j].left * width / START_RECT_MAX;
			uint16_t x_max = g_battle_options.start_rects[j].right * width / START_RECT_MAX;
			uint16_t y_min = g_battle_options.start_rects[j].top * height / START_RECT_MAX;
			uint16_t y_max = g_battle_options.start_rects[j].bottom * height / START_RECT_MAX;

			if ((g_my_user.battle_status & BS_MODE) && j == FROM_BS_ALLY(g_my_user.battle_status)) {
				for (uint16_t x=x_min; x<x_max; ++x) {
					for (uint16_t y=y_min; y<y_max; ++y) {
						if ((pixels[x+width*y] & 0x00FF00) >= (0x00FF00 - 0x003000))
							pixels[x+width*y] |= 0x00FF00;
						else
							pixels[x+width*y] += 0x003000;
					}
				}
			} else {
				for (uint16_t x=x_min; x<x_max; ++x) {
					for (uint16_t y=y_min; y<y_max; ++y) {
						if ((pixels[x+width*y] & 0xFF0000) >= (0xFF0000 - 0x300000))
							pixels[x+width*y] |= 0xFF0000;
						else
							pixels[x+width*y] += 0x300000;
					}
				}
			}
			for (uint16_t x=x_min; x<x_max; ++x) {
				pixels[x+width*y_min] = 0;
				pixels[x+width*(y_max-1)] = 0;
			}
			for (uint16_t y=y_min; y<y_max; ++y) {
				pixels[x_min+width*y] = 0;
				pixels[(x_max-1)+width*y] = 0;
			}
		}

	} else {
		uint8_t max = g_my_battle ? GetNumPlayers(g_my_battle) : 0;
		max = max < g_map_info.pos_len ? max : g_map_info.pos_len;
		for (uint16_t i=0; i<max; ++i) {
			uint16_t x_mid = g_map_info.positions[i].x * width / g_map_info.width;
			uint16_t y_mid = g_map_info.positions[i].z * height / g_map_info.height;

			for (uint16_t x=x_mid-5; x<x_mid+5; ++x)
				for (uint16_t y=y_mid-5; y<y_mid+5; ++y)
					pixels[x + width * y] = 0x00CC00;
		}
	}

cleanup:;
	HBITMAP bitmap = CreateBitmap(width, height, 1, 32, pixels);
	free(pixels);

	HDC dcSrc = CreateCompatibleDC(info->hDC);
	SelectObject(dcSrc, bitmap);
	BitBlt(info->hDC, x_offset, y_offset, width, height, dcSrc, 0, 0, SRCCOPY);

	DeleteObject(bitmap);
}

static wchar_t *
get_tooltip(const User *u)
{
	static wchar_t buf[128];
	int buffUsed = 0;

#define APPEND(...) { \
	int read = _snwprintf(buf + buffUsed, \
			sizeof(buf) / sizeof(*buf) - buffUsed - 1, \
			__VA_ARGS__); \
	\
	if (read < 0) {\
		buf[sizeof(buf) / sizeof(*buf) - 1] = '\0'; \
		return buf; \
	} \
	buffUsed += read; \
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

	const char *side_name = g_side_names[FROM_BS_SIDE(u->battle_status)];

	APPEND(L"Player %d - Team %d",
			FROM_BS_TEAM(u->battle_status),
			FROM_BS_ALLY(u->battle_status));
	if (*side_name)
		APPEND(L" - %hs", side_name);

	if (u->skill)
		APPEND(L"\nSkill: %hs", u->skill);

	if (u->battle_status & BS_HANDICAP)
		APPEND(L"\nHandicap: %d",
				FROM_BS_HANDICAP(u->battle_status));
	return buf;
#undef APPEND
}

static LRESULT
on_notify(WPARAM w_param, NMHDR *note)
{
	LVITEM item;
	item.mask = LVIF_PARAM;
	LVHITTESTINFO hit_test_info;

	switch (note->code) {

	case TTN_GETDISPINFO:
		GetCursorPos(&hit_test_info.pt);
		ScreenToClient((HWND)note->idFrom, &hit_test_info.pt);

		item.iItem = SendMessage((HWND)note->idFrom, LVM_HITTEST, 0,
				(LPARAM)&hit_test_info);

		if (item.iItem == -1)
			return 0;

		bounding_rect.left = LVIR_BOUNDS;
		SendMessage((HWND)note->idFrom, LVM_GETITEMRECT, item.iItem,
				(LPARAM)&bounding_rect);

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
		hit_test_info.pt = ((LPNMITEMACTIVATE)note)->ptAction;
		item.iItem = SendMessage(note->hwndFrom, LVM_SUBITEMHITTEST, 0,
				(LPARAM)&hit_test_info);
		if (item.iItem < 0)
			return 0;

		SendMessage(note->hwndFrom, LVM_GETITEM, 0, (LPARAM)&item);
		UserMenu_spawn((UserOrBot *)item.lParam, g_battle_room);
		return 1;

		break;
	}
	return DefWindowProc(g_battle_room, WM_NOTIFY, w_param, (LPARAM)note);
}

static LRESULT
on_command(WPARAM w_param, HWND window)
{
	HMENU menu;

	switch (w_param) {
	case MAKEWPARAM(DLG_START, BN_CLICKED):
		if (g_host_type && g_host_type->start_game)
			g_host_type->start_game();
		else
			Spring_launch();
		return 0;

	case MAKEWPARAM(DLG_CHANGE_MAP, BN_CLICKED):
		menu = CreatePopupMenu();
		for (size_t i=0; i<g_map_len; ++i)
			AppendMenuA(menu, MF_CHECKED * !strcmp(g_my_battle->map_name,  g_maps[i]), i + 1, g_maps[i]);
		POINT pt;
		GetCursorPos(&pt);
		int map_index = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y, g_battle_room, NULL);
		if (map_index > 0)
			ChangeMap(g_maps[map_index - 1]);
		DestroyMenu(menu);
		return 0;

		//TODO: This seems wrong to me. Too lazy to fix.
	case MAKEWPARAM(DLG_SPLIT_FIRST, BN_CLICKED) ... MAKEWPARAM(DLG_SPLIT_LAST, BN_CLICKED):
		MyBattle_set_split(LOWORD(w_param) - DLG_SPLIT_FIRST, SendDlgItemMessage(g_battle_room, DLG_SPLIT_SIZE, TBM_GETPOS, 0, 0));
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
		SetBattleStatus(&g_my_user, TO_BS_SIDE_MASK(LOWORD(w_param) - DLG_SIDE_FIRST), BS_SIDE);
		return 0;

	case MAKEWPARAM(DLG_MAPMODE_MINIMAP, BN_CLICKED) ... MAKEWPARAM(DLG_MAPMODE_ELEVATION, BN_CLICKED):
		InvalidateRect(GetDlgItem(g_battle_room, DLG_MINIMAP), 0, 0);
		return 0;
	}
	return 1;
}

static void
set_details(const Option *options, ssize_t option_len)
{
	HWND info_list = GetDlgItem(g_battle_room, DLG_INFOLIST);

	LVGROUP group;
	group.cbSize = sizeof(group);
	group.mask = LVGF_HEADER | LVGF_GROUPID;

	for (ssize_t i=0; i<option_len; ++i) {
		if (options[i].type != opt_section)
			continue;
		group.pszHeader = utf8to16(options[i].name);
		group.iGroupId = (intptr_t)&options[i];
		SendMessage(info_list, LVM_INSERTGROUP, -1, (LPARAM)&group);
	}

	LVITEMA item;
	item.mask = LVIF_GROUPID | LVIF_TEXT | LVIF_PARAM;
	item.iItem = INT_MAX;
	item.iSubItem = 0;

	for (ssize_t i=0; i<option_len; ++i) {
		if (options[i].type == opt_section)
			continue;

		item.pszText = options[i].name;
		item.lParam = (LPARAM)&options[i];
		item.iGroupId = (intptr_t)options[i].section;

		SendMessageA(info_list, LVM_INSERTITEMA, 0, (LPARAM)&item);
	}
}

/* static void
set_map_info(void) */
/* { */
/* HWND info_list = GetDlgItem(g_battle_room, DLG_INFOLIST); */
/* LVGROUP group; */
/* group.cbSize = sizeof(group); */
/* group.mask = LVGF_HEADER | LVGF_GROUPID; */
/* group.pszHeader = L"Map Info"; */
/* group.iGroupId = 1; */
/* SendMessage(info_list, LVM_INSERTGROUP, -1, (LPARAM)&group); */

/* LVITEM item; */

/* #define ADD_STR(s1, s2) \ */
/* item.mask = LVIF_TEXT | LVIF_GROUPID; \ */
/* item.iItem = INT_MAX; \ */
/* item.iSubItem = 0; \ */
/* item.pszText = s1; \ */
/* item.iGroupId = 1; \ */
/* \ */
/* item.iItem = SendMessage(info_list, LVM_INSERTITEM, 0, (LPARAM)&item); \ */
/* item.mask = LVIF_TEXT; \ */
/* item.iSubItem = 1; \ */
/* item.pszText = s2; \ */
/* SendMessage(info_list, LVM_SETITEM, 0, (LPARAM)&item); */

/* if (g_map_info.author[0]) */
/* ADD_STR(L"Author", utf8to16(g_map_info.author)); */
/* ADD_STR(L"Tidal: ", utf8to16(g_map_info.author)); */

/* #undef ADD_STR */
/* } */

void
BattleRoom_on_set_mod_details(void)
{
	HWND info_list = GetDlgItem(g_battle_room, DLG_INFOLIST);
	SendMessage(info_list, LVM_REMOVEALLGROUPS, 0, 0);
	ListView_DeleteAllItems(info_list);

	set_details(g_mod_options, g_mod_option_len);
	set_details(g_map_options, g_map_option_len);
	/* set_map_info(); */

	ListView_SetColumnWidth(info_list, 0, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(info_list, 1, LVSCW_AUTOSIZE_USEHEADER);
}

void
BattleRoom_on_set_option(Option *opt)
{
	HWND info_list = GetDlgItem(g_battle_room, DLG_INFOLIST);

	LVFINDINFO find_info;
	find_info.flags = LVFI_PARAM;
	find_info.lParam = (LPARAM)opt;

	LVITEMA item;
	item.mask = LVIF_TEXT;
	item.iSubItem = 1;
	item.pszText = opt->val ?: opt->def;
	item.iItem = ListView_FindItem(info_list, -1, &find_info);

	SendMessageA(info_list, LVM_SETITEMA, 0, (LPARAM)&item);
}

static void
set_side_icons(void)
{
	for (enum DialogId i=0; i<=DLG_SIDE_LAST - DLG_SIDE_FIRST; ++i) {
		HWND side_button = GetDlgItem(g_battle_room, DLG_SIDE_FIRST + i);
		IconList_set_window_image(side_button, ICON_FIRST_SIDE + i);
		ShowWindow(side_button, i < g_side_len);
	}
}

void
BattleRoom_on_change_mod(void)
{
	HWND player_list = GetDlgItem(g_battle_room, DLG_PLAYERLIST);
	LVITEM item;

	set_side_icons();

	item.mask = LVIF_PARAM;
	for (item.iItem= -1; (item.iItem = SendMessage(player_list, LVM_GETNEXTITEM, item.iItem, 0)) >= 0;) {
		item.mask = LVIF_PARAM;
		SendMessage(player_list, LVM_GETITEM, 0, (LPARAM)&item);
		item.mask = LVIF_IMAGE;
		item.iSubItem = COLUMN_SIDE;
		union UserOrBot *s = (void *)item.lParam;
		item.iImage = s->battle_status & BS_MODE
			&& *g_side_names[FROM_BS_SIDE(s->battle_status)] ? ICON_FIRST_SIDE + (int)FROM_BS_SIDE(s->battle_status) : -1;
		SendMessage(player_list, LVM_SETITEM, 0, (LPARAM)&item);
	}
}

static void on_split_size_scroll(void) {
	SplitType split_type = 0;

	/* find which split button is checked: */
	while (!SendDlgItemMessage(g_battle_room,
				DLG_SPLIT_FIRST + split_type,
				BM_GETCHECK, 0, 0)) {
		if (split_type > SPLIT_LAST) {
			assert(0);
			return;
		}
		++split_type;
	}

	int split_size = SendDlgItemMessage(g_battle_room, DLG_SPLIT_SIZE,
			TBM_GETPOS, 0, 0);
	MyBattle_set_split(split_type, split_size);
}

static LRESULT CALLBACK
battle_room_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch(msg) {

	case WM_CREATE:
		on_create(window);
		return 0;

	case WM_SIZE:
		resize_all(l_param);
		return 0;

	case WM_DRAWITEM:
		if (w_param == DLG_MINIMAP)
			on_draw((void *)l_param);
		return 1;

	case WM_NOTIFY:
		return on_notify(w_param, (NMHDR *)l_param);

	case WM_DESTROY:
		g_battle_room = NULL;
		return 0;

	case WM_HSCROLL:
		if (GetDlgCtrlID((HWND)l_param) == DLG_SPLIT_SIZE
				&& w_param == SB_ENDSCROLL)
			on_split_size_scroll();
		return 0;

	case WM_CLOSE:
		MainWindow_disable_battleroom_button();
		LeaveBattle();
		return 0;

	case WM_COMMAND:
		return on_command(w_param, (HWND)l_param);

	default:
		return DefWindowProc(window, msg, w_param, l_param);
	}
}

static void __attribute__((constructor))
init (void)
{
	WNDCLASSEX window_class = {
		.lpszClassName = WC_BATTLEROOM,
		.cbSize        = sizeof(WNDCLASSEX),
		.lpfnWndProc   = battle_room_proc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};

	RegisterClassEx(&window_class);
}

