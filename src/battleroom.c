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
#include <commctrl.h>
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
#include "minimap.h"
#include "mybattle.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "user.h"
#include "usermenu.h"
#include "wincommon.h"

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

	DLG_LAST = DLG_INFOLIST,
};

static void             _init(void);
static int              add_user_to_playerlist(union UserOrBot *u);
static LRESULT CALLBACK battle_room_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param);
static int              find_user(const void *u);
static const char *     get_effective_name(const union UserOrBot *s);
static wchar_t *        get_tooltip(const User *u);
static void             on_create(HWND window);
static LRESULT          on_command(WPARAM w_param, HWND window);
static LRESULT          on_notify(WPARAM w_param, NMHDR *note);
static void             on_size(LPARAM l_param);
static void             refresh_playerlist(void);
static void             set_details(const Option *options, ssize_t option_len);
static void             set_icon(int list_index, int column_index, IconIndex icon, IconIndex state_icon);
static void             set_player_icon(const UserOrBot *u, int list_index);
static void             set_side_icons(void);
static int CALLBACK     sort_listview(const union UserOrBot *u1, const union UserOrBot *u2, LPARAM unused);
static LRESULT CALLBACK tooltip_subclass(HWND window, UINT msg, WPARAM w_param, LPARAM l_param, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
static void             update_group(uint8_t group_id);

HWND g_battle_room;

/* If the cursor moves outside this area, then remove it */
static RECT g_tooltip_bounding_rect;

static const DialogItem DIALOG_ITEMS[] = {
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
		.class = WC_MINIMAP,
		.style = WS_VISIBLE,
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
	}
};

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
	if (s->ai)
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
	for (uint8_t i = 0; i < g_my_battle->user_len; ++i)
		players_on_team += g_my_battle->users[i]->mode
			&& g_my_battle->users[i]->ally == group_id;

	_swprintf(buf, L"Team %d :: %hu Player%c", group_id + 1,
			players_on_team, players_on_team > 1 ? 's' : '\0');

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

static int
add_user_to_playerlist(union UserOrBot *u)
{
	LVITEM item;
	wchar_t buf[MAX_NAME_LENGTH * 2 + 4];
	wchar_t *buf_end;

	assert(find_user(u) == -1);

	item.iItem = 0;
	item.iSubItem = 0;
	item.mask = LVIF_PARAM | LVIF_TEXT;
	item.pszText = buf;
	item.lParam = (LPARAM)u;

	buf_end = buf + _swprintf(buf, L"%hs", u->name);
	if (u->ai)
		_snwprintf(buf_end, LENGTH(buf) - (buf_end - buf),
				L" (%hs)", u->bot.dll);
	else
		_snwprintf(buf_end, LENGTH(buf) - (buf_end - buf),
				L" (%hs)", u->user.alias);

	return SendDlgItemMessage(g_battle_room, DLG_PLAYERLIST, LVM_INSERTITEM, 0,
			(LPARAM)&item);
}

static void
set_icon(int list_index, int column_index, IconIndex icon, IconIndex state_icon)
{
	LVITEM item;
	item.iItem = list_index;
	item.mask = state_icon == (IconIndex)-1 ? LVIF_IMAGE : LVIF_IMAGE | LVIF_STATE;
	item.iSubItem = column_index;
	item.iImage = icon != (IconIndex)-1 ? icon : -1;
	item.state = INDEXTOOVERLAYMASK(state_icon);
	SendDlgItemMessage(g_battle_room, DLG_PLAYERLIST, LVM_SETITEM,
			0, (LPARAM)&item);
}

static void
set_player_icon(const UserOrBot *u, int list_index)
{
	IconIndex side_icon;
	IconIndex status_icon;
	IconIndex status_overlay;

	side_icon = -1;
	if (u->mode && *g_side_names[u->side])
		side_icon = ICON_FIRST_SIDE + u->side;

	set_icon(list_index, COLUMN_SIDE, side_icon, -1);
	set_icon(list_index, COLUMN_COLOR, IconList_get_user_color(u), -1);
	set_icon(list_index, COLUMN_FLAG, ICON_FIRST_FLAG + u->user.country, -1);
	set_icon(list_index, COLUMN_RANK, ICON_FIRST_RANK + u->user.rank, -1);

	{
		LVITEM item;

		item.iItem = list_index;
		item.iSubItem = 0;
		item.mask = LVIF_GROUPID;
		item.iGroupId = u->mode ? u->ally : 16;
		SendDlgItemMessage(g_battle_room, DLG_PLAYERLIST, LVM_SETITEM,
				0, (LPARAM)&item);
	}

	if (u->ai)
		return;

	status_icon = &u->user == g_my_battle->founder
		? (u->mode ? ICON_HOST : ICON_HOST_SPECTATOR)
		: u->user.ingame ? ICON_INGAME
		: !u->mode ? ICON_SPECTATOR
		: u->ready ? ICON_READY
		: ICON_UNREADY;

	status_overlay = ICONMASK_USER;
	if (u->sync != SYNC_SYNCED)
		status_overlay |= ICONMASK_SYNC;
	if (u->user.away)
		status_overlay |= ICONMASK_AWAY;
	if (u->user.ignore)
		status_overlay |= ICONMASK_IGNORE;
	set_icon(list_index, COLUMN_STATUS, status_icon, status_overlay);
}

static void
refresh_playerlist(void)
{
	int team_sizes[16] = {0};

	for (uint8_t i = 0; i < g_my_battle->user_len; ++i)
		team_sizes[g_my_battle->users[i]->team]
			+= (g_my_battle->users[i]->mode) != 0;

	for (uint8_t i = 0; i < g_my_battle->user_len - g_my_battle->bot_len; ++i) {
		LVITEM item;
		User *u;
		wchar_t buf[128], *buf_end;

		buf_end = buf;
		u = &g_my_battle->users[i]->user;

		if (u->mode && team_sizes[u->team] > 1)
			buf_end += _swprintf(buf_end, L"%d: ", u->team+1);

		buf_end += _swprintf(buf_end, L"%hs", u->name);

		if (strcmp(UNTAGGED_NAME(u->name), u->alias))
			buf_end += _swprintf(buf_end, L" (%hs)", u->alias);

		item.iItem = find_user(u);
		item.mask = LVIF_TEXT;
		item.iSubItem = COLUMN_NAME;
		item.pszText = buf;

		SendDlgItemMessage(g_battle_room, DLG_PLAYERLIST, LVM_SETITEM,
				0, (LPARAM)&item);
	}

	SendDlgItemMessage(g_battle_room, DLG_PLAYERLIST, LVM_SORTITEMS, 0, (LPARAM)sort_listview);

	for (uint8_t i=0; i<16; ++i)
		update_group(i);
}

void
BattleRoom_update_user(union UserOrBot *u)
{
	int list_index;

	list_index = find_user(u);
	if (list_index < 0)
		list_index = add_user_to_playerlist(u);
	assert(list_index >= 0);

	set_player_icon(u, list_index);

	refresh_playerlist();

	if ((User *)u == &g_my_user) {
		SendDlgItemMessage(g_battle_room, DLG_SPECTATE, BM_SETCHECK,
				!u->mode, 0);

		EnableWindow(GetDlgItem(g_battle_room, DLG_AUTO_UNSPEC),
				!u->mode);

		for (size_t i=0; i<=NUM_SIDE_BUTTONS; ++i)
			SendDlgItemMessage(g_battle_room, DLG_SIDE_FIRST + i,
					BM_SETCHECK, u->side == i, 0);

		SendDlgItemMessage(g_battle_room, DLG_ALLY, CB_SETCURSEL,
				u->ally, 0);
	}

	if ((User *)u == u->battle->founder) {
		HWND start_button;

		start_button = GetDlgItem(g_battle_room, DLG_START);
		EnableWindow(start_button, !g_my_user.ingame);
	}
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
on_size(LPARAM l_param)
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
	MOVE_ID(DLG_MINIMAP, minimap_x, YS, width - minimap_x - XS, INFO_HEIGHT);
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

	EndDeferWindowPos(dwp);
	BattleRoom_resize_columns();
}

static LRESULT CALLBACK
tooltip_subclass(HWND window, UINT msg, WPARAM w_param, LPARAM l_param,
		__attribute__((unused)) UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (msg == WM_MOUSEMOVE
			&& (GET_X_LPARAM(l_param) < g_tooltip_bounding_rect.left
				|| GET_X_LPARAM(l_param) > g_tooltip_bounding_rect.right
				|| GET_Y_LPARAM(l_param) < g_tooltip_bounding_rect.top
				|| GET_Y_LPARAM(l_param) > g_tooltip_bounding_rect.bottom))
		SendMessage((HWND)dwRefData, TTM_POP, 0, 0);

	return DefSubclassProc(window, msg, w_param, l_param);
}

static void
on_create(HWND window)
{
	g_battle_room = window;
	CreateDlgItems(window, DIALOG_ITEMS, DLG_LAST + 1);
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

}

static wchar_t *
get_tooltip(const User *u)
{
	static wchar_t buf[128];
	int buf_used = 0;

#define APPEND(...) { \
	int read = _snwprintf(buf + buf_used, \
			sizeof(buf) / sizeof(*buf) - buf_used - 1, \
			__VA_ARGS__); \
	\
	if (read < 0) {\
		buf[sizeof(buf) / sizeof(*buf) - 1] = '\0'; \
		return buf; \
	} \
	buf_used += read; \
}

	APPEND(L"%hs", u->name);

	if (strcmp(UNTAGGED_NAME(u->name), u->alias))
		APPEND(L" (%hs)", u->alias);

	if (!u->ai)
		APPEND(L"\nRank %d - %hs - %.2fGHz",
				u->rank,
				Country_get_name(u->country),
				(float)u->cpu / 1000);

	if (u->skill)
		APPEND(L"\nSkill: %hs", u->skill);

	if (!u->mode) {
		APPEND(L"\nSpectator");
		return buf;
	}

	const char *side_name = g_side_names[u->side];

	APPEND(L"\nPlayer %d - Team %d",
			u->team,
			u->ally);
	if (*side_name)
		APPEND(L" - %hs", side_name);

	if (u->handicap)
		APPEND(L"\nHandicap: %d",
				u->handicap);
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

		g_tooltip_bounding_rect.left = LVIR_BOUNDS;
		SendMessage((HWND)note->idFrom, LVM_GETITEMRECT, item.iItem,
				(LPARAM)&g_tooltip_bounding_rect);

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
	}
	return DefWindowProc(g_battle_room, WM_NOTIFY, w_param, (LPARAM)note);
}

static LRESULT
on_command(WPARAM w_param, HWND window)
{
	bool button_state;
	BattleStatus new_battle_status;

	switch (w_param) {
	case MAKEWPARAM(DLG_START, BN_CLICKED):
		if (g_host_type && g_host_type->start_game)
			g_host_type->start_game();
		else
			Spring_launch();
		return 0;

	case MAKEWPARAM(DLG_LEAVE, BN_CLICKED):
		SendMessage(g_battle_room, WM_CLOSE, 0, 0);
		return 0;

	case MAKEWPARAM(DLG_SPECTATE, BN_CLICKED):
		button_state = Button_GetCheck(window);
		if (!button_state)
			SendDlgItemMessage(g_battle_room, DLG_AUTO_UNSPEC,
					BM_SETCHECK, BST_UNCHECKED, 0);

		new_battle_status = g_last_battle_status;
		new_battle_status.mode = button_state;

		SetMyBattleStatus(new_battle_status);
		return 0;

	case MAKEWPARAM(DLG_ALLY, CBN_SELCHANGE):
		new_battle_status = g_last_battle_status;
		new_battle_status.ally = SendMessage(window, CB_GETCURSEL, 0, 0);
		SetMyBattleStatus(new_battle_status);
		SendMessage(window, CB_SETCURSEL, g_my_user.ally, 0);
		return 0;

	case MAKEWPARAM(DLG_SIDE_FIRST, BN_CLICKED) ... MAKEWPARAM(DLG_SIDE_LAST, BN_CLICKED):
		new_battle_status = g_last_battle_status;
		new_battle_status.side = LOWORD(w_param) - DLG_SIDE_FIRST;
		SetMyBattleStatus(new_battle_status);
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

void
BattleRoom_on_set_mod_details(void)
{
	HWND info_list = GetDlgItem(g_battle_room, DLG_INFOLIST);
	SendMessage(info_list, LVM_REMOVEALLGROUPS, 0, 0);
	ListView_DeleteAllItems(info_list);

	set_details(g_mod_options, g_mod_option_len);
	set_details(g_map_options, g_map_option_len);

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
		item.iImage = s->mode
			&& *g_side_names[s->side] ? ICON_FIRST_SIDE + (int)s->side : -1;
		SendMessage(player_list, LVM_SETITEM, 0, (LPARAM)&item);
	}
}

static LRESULT CALLBACK
battle_room_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch(msg) {

	case WM_CREATE:
		on_create(window);
		return 0;

	case WM_SIZE:
		on_size(l_param);
		return 0;

	case WM_NOTIFY:
		return on_notify(w_param, (NMHDR *)l_param);

	case WM_DESTROY:
		g_battle_room = NULL;
		return 0;

	case WM_CLOSE:
		MainWindow_disable_battleroom_button();
		MyBattle_leave();
		return 0;

	case WM_COMMAND:
		return on_command(w_param, (HWND)l_param);

	default:
		return DefWindowProc(window, msg, w_param, l_param);
	}
}

static void __attribute__((constructor))
_init (void)
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
