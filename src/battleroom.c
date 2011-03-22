

#include <stdio.h>
#include <math.h>
#include <limits.h>

#include "wincommon.h"

#include <Commctrl.h>
#include <Wingdi.h>
#include <assert.h>
#include <richedit.h>


#include "battleroom.h"
#include "chat.h"
#include "usermenu.h"
#include "client_message.h"
#include "chat.h"
#include "data.h"
#include "countrycodes.h"
#include "listview.h"
#include "imagelist.h"
#include "dialogboxes.h"
#include "sync.h"
#include "common.h"
#include "settings.h"
#include "layoutmetrics.h"
#include "downloader.h"
#include "battletools.h"

#include "spring.h"

#include "resource.h"

HWND gBattleRoomWindow;

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
	DLG_ALLY,
	DLG_PLAYER_LIST,
	DLG_SIDE,
	DLG_SPECTATE,
	DLG_READY,
	DLG_MINIMAP,
	DLG_TOOLS,
	DLG_START,
	DLG_LEAVE,
	DLG_BATTLE_INFO,
	
	//Single Player Only:
	DLG_MOD,
	DLG_MAP,

	DLG_LAST = DLG_MAP,
	DLG_STARTPOS,
	DLG_FIRST_STARTPOS = DLG_STARTPOS,
};

static const DialogItem dlgItems[] = {
	[DLG_CHAT] = {
		.class = WC_CHATBOX,
		.style = WS_VISIBLE,
	}, [DLG_ALLY] = {
		.class = WC_COMBOBOX,
		.style = WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
	}, [DLG_MOD] = {
		.class = WC_COMBOBOX,
		.style = WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
	}, [DLG_MAP] = {
		.class = WC_COMBOBOX,
		.style = WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
	}, [DLG_SIDE] = {
		.class = WC_COMBOBOXEX,
		.name = L"Side",
		.style = WS_VISIBLE | CBS_DROPDOWNLIST | LVS_SHAREIMAGELISTS | WS_VSCROLL,
	}, [DLG_READY] = {
		.class = WC_BUTTON,
		.name = L"Ready",
		.style = WS_VISIBLE | BS_CHECKBOX,
	}, [DLG_SPECTATE] = {
		.class = WC_BUTTON,
		.name = L"Spectate",
		.style = WS_VISIBLE | BS_CHECKBOX,
	}, [DLG_MINIMAP] = {
		.class = WC_STATIC,
		.style = WS_VISIBLE | SS_BITMAP | SS_REALSIZECONTROL,
	}, [DLG_LEAVE] = {
		.class = WC_BUTTON,
		.name = L"Leave",
		.style = WS_VISIBLE | BS_PUSHBUTTON,
	}, [DLG_START] = {
		.class = WC_BUTTON,
		.name = L"Start",
		.style = WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
	}, [DLG_TOOLS] = {
		.class = WC_BUTTON,
		.name = L"Tools",
		.style = WS_VISIBLE | BS_PUSHBUTTON,
	}, [DLG_PLAYER_LIST] = {
		.class = WC_LISTVIEW,
		.exStyle = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | LVS_SHAREIMAGELISTS | LVS_SINGLESEL | LVS_REPORT | LVS_NOCOLUMNHEADER,
	}, [DLG_BATTLE_INFO] = {
		.class = RICHEDIT_CLASS,
		.style = ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_VISIBLE,
	}, [DLG_STARTPOS] = {
		.class = WC_BUTTON,
		.style = WS_VISIBLE | BS_PUSHBUTTON | BS_ICON,
	},
};

void BattleRoom_Show(void)
{
	int isSP = gBattleOptions.hostType == HOST_SP;
	ShowWindow(GetDlgItem(gBattleRoomWindow, DLG_CHAT), !isSP);
	ShowWindow(GetDlgItem(gBattleRoomWindow, DLG_PLAYER_LIST), !isSP);
	ShowWindow(GetDlgItem(gBattleRoomWindow, DLG_READY), !isSP);
	ShowWindow(GetDlgItem(gBattleRoomWindow, DLG_ALLY), !isSP);
	ShowWindow(GetDlgItem(gBattleRoomWindow, DLG_MAP), isSP);
	ShowWindow(GetDlgItem(gBattleRoomWindow, DLG_MOD), isSP);
	if (isSP)
		ShowWindow(GetDlgItem(gBattleRoomWindow, DLG_MINIMAP), 1);
	SetDlgItemText(gBattleRoomWindow, DLG_TOOLS, !isSP ? L"Tools" : L"Add bot");
	EnableWindow(GetDlgItem(gBattleRoomWindow, DLG_START), isSP);
	RECT rect;
	GetClientRect(gBattleRoomWindow, &rect);
	SendMessage(gBattleRoomWindow, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
	FocusTab(gBattleRoomWindow);
}

void BattleRoom_Hide(void)
{
	RemoveTab(gBattleRoomWindow);
	ListView_DeleteAllItems(GetDlgItem(gBattleRoomWindow, DLG_PLAYER_LIST));
}

static int findUser(const void *u)
{
	return SendDlgItemMessage(gBattleRoomWindow, DLG_PLAYER_LIST, LVM_FINDITEM, -1,
			(LPARAM)&(LVFINDINFO){.flags = LVFI_PARAM, .lParam = (LPARAM)u});
}

static void *getUserFromIndex(int index)
{
	LVITEM item = {
		.mask = LVIF_PARAM,
		.iItem = index,
	};
	SendDlgItemMessage(gBattleRoomWindow, DLG_PLAYER_LIST, LVM_GETITEM, 0, (LPARAM)&item);
	return (void *)item.lParam;
}

void BattleRoom_RemoveUser(const union UserOrBot *s)
{
	SendDlgItemMessage(gBattleRoomWindow, DLG_PLAYER_LIST, LVM_DELETEITEM, findUser(s), 0);
}

static void setIcon(int item, int subItem, enum ICONS icon) {
	SendDlgItemMessage(gBattleRoomWindow, DLG_PLAYER_LIST, LVM_SETITEM, 0, (LPARAM)&(LVITEM){.iItem = item, .iSubItem = subItem, .mask = LVIF_IMAGE, .iImage = icon});
}

const char * getEffectiveUsername(const union UserOrBot *s)
{
	if (s->battleStatus & AI_MASK)
		return s->bot.owner->name;
	return s->name;
}

int CALLBACK sortPlayerList(const union UserOrBot *u1, const union UserOrBot *u2, LPARAM unused)
{
	return strcmpi(getEffectiveUsername(u1), getEffectiveUsername(u2));
}

void BattleRoom_UpdateUser(union UserOrBot *s)
{
	HWND playerListHandle = GetDlgItem(gBattleRoomWindow, DLG_PLAYER_LIST);

	int item = findUser(s);
	uint32_t battleStatus = s->battleStatus;

	
	item = item == -1
			? SendMessage(playerListHandle, LVM_INSERTITEM,
				0, (LPARAM)&(LVITEM){.mask = LVIF_PARAM, .lParam = (LPARAM)s})
			: item;

	ListView_SetItem(playerListHandle, (&(LVITEM){
		.iItem = item,
		.mask = LVIF_GROUPID,
		.iGroupId = (battleStatus & MODE_MASK) ? FROM_ALLY_MASK(battleStatus) : 16,
	}));

	SendMessage(playerListHandle, LVM_SETITEM, 0, (LPARAM)&(LVITEM){
		.iItem = item, .iSubItem = COLUMN_STATUS, .mask = LVIF_STATE | LVIF_IMAGE,
		.iImage = battleStatus & AI_MASK ? 0 
		    : &s->user == gMyBattle->founder ? (battleStatus & MODE_MASK ? ICONS_HOST : ICONS_HOST_SPECTATOR)
			: s->user.clientStatus & CS_INGAME_MASK ? ICONS_INGAME
			: !(battleStatus & MODE_MASK) ? ICONS_SPECTATOR
			: battleStatus & READY_MASK ? ICONS_READY
			: ICONS_UNREADY,
		.stateMask = LVIS_OVERLAYMASK,
		.state = battleStatus & AI_MASK ? 0 :
				INDEXTOOVERLAYMASK(USER_MASK
				   | !(battleStatus & SYNCED) * UNSYNC_MASK
				   | (s->user.clientStatus & CS_AWAY_MASK) * AWAY_MASK >> AWAY_OFFSET
				   | s->user.ignore * IGNORE_MASK)});

	setIcon(item, COLUMN_SIDE, battleStatus & MODE_MASK && *gSideNames[FROM_SIDE_MASK(battleStatus)] ? ICONS_FIRST_SIDE + FROM_SIDE_MASK(battleStatus) : -1);

	extern int GetColorIndex(const union UserOrBot *);
	setIcon(item, COLUMN_COLOR, GetColorIndex((void *)s));
	
	if (battleStatus & AI_MASK) {
		wchar_t name[MAX_NAME_LENGTH * 2 + 4];
		swprintf(name, L"%hs (%hs)", s->name, s->bot.dll);
		SendMessage(playerListHandle, LVM_SETITEM, 0, (LPARAM)&(LVITEM){
				LVIF_TEXT, item, COLUMN_NAME,
				.pszText = name});
		goto sort;
	}
	
	User *u = &s->user;
	
	setIcon(item, COLUMN_FLAG, ICONS_FIRST_FLAG + u->country);
	setIcon(item, COLUMN_RANK, ICONS_FIRST_RANK + FROM_RANK_MASK(u->clientStatus));

	if (u == gMyUser) {
		SendDlgItemMessage(gBattleRoomWindow, DLG_READY, BM_SETCHECK, !!(battleStatus & READY_MASK), 0);
		SendDlgItemMessage(gBattleRoomWindow, DLG_SPECTATE, BM_SETCHECK, !(battleStatus & MODE_MASK), 0);
		SendDlgItemMessage(gBattleRoomWindow, DLG_SIDE, CB_SETCURSEL, FROM_SIDE_MASK(battleStatus), 0);
		SendDlgItemMessage(gBattleRoomWindow, DLG_ALLY, CB_SETCURSEL, FROM_ALLY_MASK(battleStatus), 0);
	}
	if (u->battle->founder == u)
		EnableWindow(GetDlgItem(gBattleRoomWindow, DLG_START), !!(u->clientStatus & CS_INGAME_MASK) ^ (u == gMyUser) || gBattleOptions.hostType & HOST_FLAG);

	int teamSizes[16] = {};
	FOR_EACH_PLAYER(u, gMyBattle)
		++teamSizes[FROM_TEAM_MASK(u->battleStatus)];
	FOR_EACH_USER(u, gMyBattle) {
		wchar_t buff[128], *s=buff;
		if ((u->battleStatus & MODE_MASK) && teamSizes[FROM_TEAM_MASK(u->battleStatus)] > 1)
			s += swprintf(s, L"%d: ", FROM_TEAM_MASK(u->battleStatus)+1);
		s += swprintf(s, L"%hs", u->name);
		if (strcmp(u->name, u->alias))
			s += swprintf(s, L" (%hs)", u->alias);
		SendMessage(playerListHandle, LVM_SETITEM, 0, (LPARAM)&(LVITEM){
				.mask = LVIF_TEXT,
				.iItem = findUser(u),
				.iSubItem = COLUMN_NAME,
				.pszText = buff});
	}
	sort:
	SendMessage(playerListHandle, LVM_SORTITEMS, 0, (LPARAM)sortPlayerList);
}

void resizePlayerListTabs(void)
{
	HWND playerList = GetDlgItem(gBattleRoomWindow, DLG_PLAYER_LIST);
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

static void resizeAll(LPARAM lParam)
{
	#define INFO_MARGIN 140
	#define LIST_MARGIN 200
	#define S 2
	int w = LOWORD(lParam), h = HIWORD(lParam);
	int battleInfoMargin = MAP_X(INFO_MARGIN);
	int playerListMargin = MAP_X(LIST_MARGIN);
	

	int mode = gBattleOptions.hostType == HOST_SP ? 4
			 : w - battleInfoMargin < playerListMargin ? 0
	         : w - battleInfoMargin < 2*playerListMargin ? 1
			 : w - playerListMargin < 2.5*playerListMargin ? 2
			 : 3;
			 
	HDWP dwp = BeginDeferWindowPos(DLG_LAST + 1);
		
	#define MOVE(window, x, y, cx, cy)\
		(DeferWindowPos(dwp, (window), NULL, (x), (y), (cx), (cy), 0))
	#define MOVEID(id, x, y, cx, cy)\
		MOVE(GetDlgItem(gBattleRoomWindow, (id)), (x), (y), (cx), (cy))
		
	
	MOVEID(DLG_TOOLS, w - MAP_X(171), h - MAP_Y(14 + S), MAP_X(50), MAP_Y(14));
	MOVEID(DLG_START, w - MAP_X(114), h - MAP_Y(14 + S), MAP_X(50), MAP_Y(14));
	MOVEID(DLG_LEAVE, w - MAP_X(57), h - MAP_Y(14 + S), MAP_X(50), MAP_Y(14));
	
	if (mode == 4) {
		DeferWindowPos(dwp, GetDlgItem(gBattleRoomWindow, DLG_BATTLE_INFO), NULL, 0, 0, battleInfoMargin, h - MAP_Y(14 + 7 + 2), SWP_SHOWWINDOW);
		DeferWindowPos(dwp, GetDlgItem(gBattleRoomWindow, DLG_MINIMAP), NULL, battleInfoMargin, 0, w - battleInfoMargin, h - MAP_Y(14 + 7 + 2), 0);
		int combospace = (w - MAP_X(INFO_MARGIN + 171 + 3 * 7))/2;
		MOVEID(DLG_MAP, MAP_X(INFO_MARGIN + 7), h - MAP_Y(14 + S), combospace, MAP_Y(14));
		MOVEID(DLG_MOD, MAP_X(INFO_MARGIN + 14) + combospace, h - MAP_Y(14 + S), combospace, MAP_Y(14));
		MOVEID(DLG_SPECTATE, 0,	h - MAP_Y(14 + S), MAP_X(70), COMMANDBUTTON_Y);
		MOVEID(DLG_SIDE, MAP_X(70), h - MAP_Y(14 + S), MAP_X(70), COMMANDBUTTON_Y);
		goto done;
	}

	int minimapX = mode == 3 ? w - playerListMargin : (mode != 1) * (w - playerListMargin);
	int minimapY = mode == 3 ?  0 : (mode == 1) * (h - battleInfoMargin);
	int minimapW = mode == 1 ? battleInfoMargin : playerListMargin;

	if (!mode) {
		MOVEID(DLG_CHAT, 0, playerListMargin + 0, w, h - (2 * COMMANDBUTTON_Y) - playerListMargin - 2 * 0);
		MOVEID(DLG_PLAYER_LIST, 0, 0, w, playerListMargin);
	} else if (mode == 3) {
		MOVEID(DLG_CHAT, MAP_X(INFO_MARGIN + S), 0, w - MAP_X(INFO_MARGIN + 2*S + LIST_MARGIN), h);
		MOVEID(DLG_PLAYER_LIST, w - MAP_X(LIST_MARGIN), minimapW, playerListMargin, h - playerListMargin - MAP_Y(14 + 2*S));
	} else {
		MOVEID(DLG_CHAT, MAP_X(INFO_MARGIN + S), playerListMargin , w - MAP_X(INFO_MARGIN + S), h - MAP_Y(14 + 2*S) - playerListMargin);
		MOVEID(DLG_PLAYER_LIST, battleInfoMargin, 0, w - battleInfoMargin - (mode != 1) * playerListMargin, playerListMargin);
	}
	MOVEID(DLG_READY, 0,
			!mode ? h - 2 * COMMANDBUTTON_Y : 0,
			MAP_X(70), COMMANDBUTTON_Y);
	MOVEID(DLG_SPECTATE, 0,
			!mode ? h - COMMANDBUTTON_Y : 0 + COMMANDBUTTON_Y,
			MAP_X(70), COMMANDBUTTON_Y);
	MOVEID(DLG_ALLY, 2 * 0 + MAP_X(70),
			!mode ? h - 2 * COMMANDBUTTON_Y : 0,
			MAP_X(70), COMMANDBUTTON_Y);
	MOVEID(DLG_SIDE, 2 * 0 + MAP_X(70),
			!mode ? h - COMMANDBUTTON_Y : 0 + COMMANDBUTTON_Y,
			MAP_X(70), COMMANDBUTTON_Y);

	DeferWindowPos(dwp, GetDlgItem(gBattleRoomWindow, DLG_MINIMAP), NULL, minimapX, minimapY, minimapW, minimapW, mode ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);

	DeferWindowPos(dwp, GetDlgItem(gBattleRoomWindow, DLG_BATTLE_INFO), NULL, 0, MAP_Y(28), battleInfoMargin, h - MAP_Y(28) - (mode == 1)*minimapW, !mode ? SWP_HIDEWINDOW : SWP_SHOWWINDOW);
	
	done:
	EndDeferWindowPos(dwp);
	resizePlayerListTabs();
	SendMessage(gBattleRoomWindow, WM_MOVESTARTPOSITIONS, 0, 0);
}

static RECT boundingRect;
static LRESULT CALLBACK playerListProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (msg == WM_MOUSEMOVE
			&& (GET_X_LPARAM(lParam) < boundingRect.left
				|| GET_X_LPARAM(lParam) > boundingRect.right
				|| GET_Y_LPARAM(lParam) < boundingRect.top
				|| GET_Y_LPARAM(lParam) > boundingRect.bottom))
		SendMessage((HWND)dwRefData, TTM_POP, 0, 0);
	return DefSubclassProc(window, msg, wParam, lParam);
}

static LRESULT CALLBACK startPositionProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR teamID)
{
	static LPARAM startPos;
	if (!(gBattleOptions.hostType & HOST_FLAG))
		goto done;
	switch (msg) {
	case WM_LBUTTONDOWN:
		startPos = lParam;
		break;
	case WM_LBUTTONUP:
		if (!startPos)
			break;
		if (startPos == -1) {
			RECT rect, rect2;
			GetWindowRect(window, &rect);
			GetWindowRect(GetParent(window), &rect2);
			int w = gMapInfo.width * (rect.left + rect.right - 2 * rect2.left) / (rect2.right - rect2.left) / 2;
			w = max(min(w, gMapInfo.width - 4), 4);
			int h = gMapInfo.height * (rect.top + rect.bottom - 2 * rect2.top) / (rect2.bottom - rect2.top) / 2;
			h = max(min(h, gMapInfo.height - 4), 4);
			if (gBattleOptions.hostType == HOST_SP)
				gBattleOptions.positions[teamID] = (StartPos){w, h};
			else
				SendToServer("!SETSCRIPTTAGS game/team%d/startposx=%d\tgame/team%d/startposy=%d", teamID, w, teamID, h);
			SendMessage(gBattleRoomWindow, WM_MOVESTARTPOSITIONS, 0, 0);
		} else
			CreateUserMenu((union UserOrBot *)GetWindowLongPtr(window, GWLP_USERDATA), window);
		startPos = 0;
		break;
	case WM_MOUSEMOVE:
		if (startPos
			&& (abs(LOWORD(startPos) - LOWORD(lParam)) > 10
				|| abs(HIWORD(startPos) - HIWORD(lParam)) > 10)) {
			startPos = -1;
			if (gBattleOptions.startPosType != STARTPOS_CHOOSE_BEFORE)
				break;
			RECT rect;
			GetClientRect(window, &rect);
			rect.right =  GET_X_LPARAM(lParam) - rect.right/2;
			rect.bottom = GET_Y_LPARAM(lParam) - rect.bottom/2;
			MapWindowPoints(window, GetParent(window), (POINT *)&rect.right, 1);
			SetWindowPos(window, NULL, rect.right, rect.bottom, 0, 0, SWP_NOSIZE);
		}
		break;
	}
	done:
	return DefSubclassProc(window, msg, wParam, lParam);
}

static LRESULT CALLBACK battleRoomProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CREATE:
		gBattleRoomWindow = window;
		CreateDlgItems(window, dlgItems, DLG_LAST + 1);
		for (int i=0; i<16; ++i) {
			HWND minimap = GetDlgItem(window, DLG_MINIMAP);
			HWND startPos = CreateDlgItem(minimap, &dlgItems[DLG_STARTPOS], DLG_FIRST_STARTPOS + i);
			SetWindowSubclass(startPos, startPositionProc, i, i);
		}
		for (int i=0; i<16; ++i) {
			wchar_t buff[lengthof("Team 16")];
			swprintf(buff, L"Team %d", i+1);
			SendDlgItemMessage(window, DLG_ALLY, CB_ADDSTRING, 0, (LPARAM)buff);
		}
		SendDlgItemMessage(window, DLG_SIDE, CBEM_SETIMAGELIST, 0, (LPARAM)iconList);
		//PlayerList
		HWND playerList = GetDlgItem(window, DLG_PLAYER_LIST);
		for (int i=0; i <= COLUMN_LAST; ++i)
			SendMessage(playerList, LVM_INSERTCOLUMN, i, (LPARAM)&(LVCOLUMN){});

		ListView_SetExtendedListViewStyle(playerList, LVS_EX_DOUBLEBUFFER |  LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
		SendMessage(playerList, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)iconList);
		ListView_EnableGroupView(playerList, TRUE);
		for (int i=0; i<=16; ++i) {
			wchar_t buff[lengthof("Spectators")];
			swprintf(buff, i<16 ? L"Team %d" : L"Spectators", i+1);
			ListView_InsertGroup(playerList, -1, (&(LVGROUP){
				.cbSize = sizeof(LVGROUP),
				.mask = LVGF_HEADER | LVGF_GROUPID,
				.pszHeader = buff,
				.iGroupId = i})
			);
		}

		HWND hwndTip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL, 0,
			0, 0, 0, 0,
			window, NULL, NULL, NULL);
		SendMessage(hwndTip, TTM_SETMAXTIPWIDTH, 0, 200);
		TOOLINFO toolInfo = {
			sizeof(toolInfo), TTF_SUBCLASS | TTF_IDISHWND | TTF_TRANSPARENT , window, (UINT_PTR)GetDlgItem(window, DLG_PLAYER_LIST),
			.lpszText = LPSTR_TEXTCALLBACK,
		};
		SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
		SetWindowSubclass(playerList, playerListProc, 0, (DWORD_PTR)hwndTip);

		SendDlgItemMessage(window, DLG_BATTLE_INFO, EM_SETEVENTMASK, 0, ENM_LINK);
		// SendDlgItemMessage(window, DLG_BATTLE_INFO, EM_AUTOURLDETECT, 1, 0);
		return 0;
	case WM_SIZE:
		resizeAll(lParam);
		if (!wParam)
			break;
		//FALLTHROUGH:
	case WM_EXITSIZEMOVE:
		InvalidateRect(GetDlgItem(gBattleRoomWindow, DLG_MINIMAP), 0, 0);
		break;
	case WM_NOTIFY: {
		const LPNMHDR note = (void *)lParam;
		if (note->code == TTN_GETDISPINFO) {
			LVHITTESTINFO info = {};
			GetCursorPos(&info.pt);
			ScreenToClient((HWND)note->idFrom, &info.pt);

			int index = SendMessage((HWND)note->idFrom, LVM_HITTEST, 0, (LPARAM)&info);

			boundingRect.left = LVIR_BOUNDS;
			SendMessage((HWND)note->idFrom, LVM_GETITEMRECT, index, (LPARAM)&boundingRect);

			User *u = getUserFromIndex(index);
			if (!u)
				return 0;
			uint32_t battleStatus = u->battleStatus;
			static wchar_t buff[1024];
			wchar_t *s=buff;
			s += swprintf(s, L"%hs", u->name);
			if (strcmp(u->name, u->alias))
				s += swprintf(s, L" (%hs)", u->alias);
			if (!(u->battleStatus & AI_MASK))
				s += swprintf(s, L"\nRank %d - %hs - %.2fGHz\n", FROM_RANK_MASK(u->clientStatus), countryNames[u->country], (float)u->cpu / 1000);
			if (battleStatus & MODE_MASK) {
				s += swprintf(s, L"Player %d - Team %d", FROM_TEAM_MASK(battleStatus), FROM_ALLY_MASK(battleStatus));
				const char *sideName = gSideNames[FROM_SIDE_MASK(battleStatus)];
				if (*sideName)
					s += swprintf(s, L" - %hs", sideName);
				if (battleStatus & HANDICAP_MASK)
					s += swprintf(s, L"\nHandicap: %d", FROM_HANDICAP_MASK(battleStatus));
			}
			else
				s += swprintf(s, L"Spectator");
			((NMTTDISPINFO *)lParam)->lpszText = buff;
			return 0;
		}
		switch (note->idFrom) {
		case DLG_BATTLE_INFO: {
			ENLINK *link = (ENLINK *)lParam;
			if (note->code != EN_LINK || link->msg != WM_LBUTTONUP)
				break;
			SendMessage(note->hwndFrom, EM_EXSETSEL, 0, (LPARAM)&link->chrg);
			wchar_t buff[link->chrg.cpMax - link->chrg.cpMin];
			SendMessage(note->hwndFrom, EM_GETSELTEXT, 0, (LPARAM)buff);

			int i = _wtoi(buff + lengthof(L":<") - 1);
			if (i == RELOAD_MAPS_MODS)
				ReloadMapsAndMod();
			else if (i == DOWNLOAD_MOD)
				DownloadMod(gMyBattle->modName);
			else if (i == DOWNLOAD_MAP)
				DownloadMap(gMyBattle->mapName);
			else
				ChangeOption(i);
		}	return 0;
		case DLG_PLAYER_LIST: {
			switch (note->code) {
			case LVN_ITEMACTIVATE:
				FocusTab(GetPrivateChat(getUserFromIndex(((LPNMITEMACTIVATE)lParam)->iItem)));
				return 1;
			case NM_RCLICK: {
				POINT pt =  ((LPNMITEMACTIVATE)lParam)->ptAction;
				int index = SendMessage(note->hwndFrom, LVM_SUBITEMHITTEST, 0, (LPARAM)&(LVHITTESTINFO){.pt = pt});
				if (index < 0)
					return 0;

				CreateUserMenu(getUserFromIndex(index), window);
			}	return 1;
			}
		}	break;
		}
		break;
	} case WM_DESTROY:
		gBattleRoomWindow = NULL;
		break;
	case WM_CLOSE:
		close:
		LeaveBattle();
		RemoveTab(window);
		return 0;
	case WM_COMMAND:
		switch (wParam) {
		case MAKEWPARAM(DLG_START, BN_CLICKED):
			LaunchSpring();
			return 0;
		case MAKEWPARAM(DLG_TOOLS, BN_CLICKED): {
			enum {
				CHANGE_COLOR = 1,
				RING_UNREADY, SPEC_UNREADY,
				
				
				//Keep order from settings.h:
				FIX_ALLIES, FIX_RANK, FIX_CLANS, FIX_IDS, FIX_COLORS,
				
				
				BALANCE_RANDOM, BALANCE_RANK, BALANCE_CLANS,
				TEAM_FLAG = 0x100,
				AI_FLAG = 0x200,
				MAP_FLAG = 0x400,
				SPLIT_FLAG = 0x800,
			};

		
			HMENU aiMenu = CreatePopupMenu();
			HMENU menu;
			int i=0;
			void appendAi(const char *name, void *aiMenu)
			{
				AppendMenuA(aiMenu, 0, AI_FLAG | i++, name);
			}
			ForEachAiName(appendAi, aiMenu);
			
			if (gBattleOptions.hostType == HOST_SP) {
				menu = aiMenu;
				goto domenu;
			}
			
			HMENU splitMenus[4];
			

			int currentSplit = gBattleOptions.startPosType != STARTPOS_CHOOSE_INGAME ? -1
				: !memcmp(&gBattleOptions.startRects[0], &(RECT){0, 0, 200 - gBattleOptions.startRects[1].left, 200}, sizeof(RECT)) ? 0
				: !memcmp(&gBattleOptions.startRects[0], &(RECT){0, 0, 200, 200 - gBattleOptions.startRects[1].top}, sizeof(RECT)) ? 1
				: !memcmp(&gBattleOptions.startRects[0], &(RECT){0, 0, 200 - gBattleOptions.startRects[1].left, 200 - gBattleOptions.startRects[1].top}, sizeof(RECT)) ? 2
				: !memcmp(&gBattleOptions.startRects[0], &(RECT){200 - gBattleOptions.startRects[1].right, 0, 200, 200 - gBattleOptions.startRects[1].top}, sizeof(RECT)) ? 3 : -1;

			for (int i=0; i<lengthof(splitMenus); ++i) {
				splitMenus[i] = CreatePopupMenu();
				for (int j=10; j<=50; j += 5) {
					wchar_t buff[128];
					swprintf(buff, L"%d", j);
					AppendMenu(splitMenus[i], MF_CHECKED * (currentSplit == i && gBattleOptions.startRects[0].right/2 == j), ((i + SPLIT_FIRST + 1) * SPLIT_FLAG) + j, buff);
					if (currentSplit == i && gBattleOptions.startRects[0].right/2%5 && gBattleOptions.startRects[0].right/10 == j/5) {
						swprintf(buff, L"%d", gBattleOptions.startRects[0].right/2);
						AppendMenu(splitMenus[i], MF_CHECKED, ((i + SPLIT_FIRST + 1) * SPLIT_FLAG) + gBattleOptions.startRects[0].right/2, buff);
					}
				}
			}
			HMENU teamMenu = CreatePopupMenu();
			for (int i=0; i<16; ++i) {
				wchar_t buff[3];
				swprintf(buff, L"%d", i+1);
				AppendMenu(teamMenu, MF_CHECKED * (i == FROM_TEAM_MASK(gMyUser->battleStatus)), TEAM_FLAG | i, buff);
			}


			menu = CreatePopupMenu();
			HMENU mapMenu = CreatePopupMenu();

			i=0;
			void appendMap(const char *name)
			{
				AppendMenuA(mapMenu, MF_CHECKED * !strcmp(gMyBattle->mapName, name), MAP_FLAG | i++, name);
			}
			ForEachMapName(appendMap);
			
			AppendMenu(menu, MF_POPUP, (UINT_PTR)mapMenu, L"Change map");
			AppendMenu(menu, MF_POPUP, (UINT_PTR)aiMenu, L"Add bot");
			AppendMenu(menu, 0, RING_UNREADY, L"Ring unready");
			if (gBattleOptions.hostType & HOST_FLAG) {
				AppendMenu(menu, 0, SPEC_UNREADY, L"Force spectate unready");
				AppendMenu(menu, MF_CHECKED * gSettings.flags & SETTING_HOST_FIX_COLOR, FIX_COLORS, L"Fix colors");
				AppendMenu(menu, MF_CHECKED * gSettings.flags & SETTING_HOST_FIX_ID, FIX_IDS, L"Fix IDs");

				AppendMenu(menu, MF_CHECKED * gSettings.flags & SETTING_HOST_BALANCE, FIX_ALLIES, L"Enable autobalance");
				AppendMenu(menu, MF_CHECKED * gSettings.flags & SETTING_HOST_BALANCE_RANK, FIX_RANK, L"Use ranks when balancing");
				AppendMenu(menu, MF_CHECKED * gSettings.flags & SETTING_HOST_BALANCE_CLAN, FIX_CLANS, L"Respect clan tags when balancing");
			} else {
				AppendMenu(menu, 0, BALANCE_RANK, L"Use ranks when balancing");
				AppendMenu(menu, 0, BALANCE_CLANS, L"Use ranks and clan tags when balancing");
				AppendMenu(menu, 0, BALANCE_RANDOM, L"Ignore ranks and clan tags when balancing");
			}


			AppendMenu(menu, MF_SEPARATOR, 0, NULL);
			AppendMenu(menu, MF_CHECKED * (gBattleOptions.startPosType == STARTPOS_FIXED), (STARTPOS_FIXED + 1) * SPLIT_FLAG, L"Fixed start positions");
			AppendMenu(menu, MF_CHECKED * (gBattleOptions.startPosType == STARTPOS_RANDOM), (STARTPOS_RANDOM + 1) * SPLIT_FLAG, L"Random start positions");
			AppendMenu(menu, MF_CHECKED * (gBattleOptions.startPosType == STARTPOS_CHOOSE_BEFORE), (STARTPOS_CHOOSE_BEFORE + 1) * SPLIT_FLAG, L"Choose start positions before game");
			
			AppendMenu(menu, MFS_CHECKED * (currentSplit == 0) | MF_POPUP, (UINT_PTR)splitMenus[0], L"East vs west");
			AppendMenu(menu, MFS_CHECKED * (currentSplit == 1) | MF_POPUP, (UINT_PTR)splitMenus[1], L"North vs south");
			AppendMenu(menu, MFS_CHECKED * (currentSplit == 2) | MF_POPUP, (UINT_PTR)splitMenus[2], L"Northwest vs southeast");
			AppendMenu(menu, MFS_CHECKED * (currentSplit == 3) | MF_POPUP, (UINT_PTR)splitMenus[3], L"Northeast vs southwest");
			// AppendMenu(menu, MFS_CHECKED * (currentSplit == 0) | MF_POPUP, 0, L"Custom split");
			
			if (gMyUser->battleStatus & MODE_MASK) {
				AppendMenu(menu, MF_SEPARATOR, 0, NULL);
				AppendMenu(menu, MF_POPUP, (UINT_PTR)teamMenu, L"Change ID");
				AppendMenu(menu, MF_POPUP, CHANGE_COLOR, L"Change Color");
			}

			domenu:;
			POINT pt;
			GetCursorPos(&pt);
			int clicked = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y, window, NULL);
			switch (clicked) {
			case CHANGE_COLOR:
				CreateColorDlg((union UserOrBot *)gMyUser);
				break;
			case RING_UNREADY:
				if (gBattleOptions.hostType == HOST_SPADS)
					SpadsMessageF("!ring");
				else {
					FOR_EACH_HUMAN_PLAYER(p, gMyBattle) {
						if (!(p->battleStatus & READY_MASK))
							SendToServer("!RING %s", p);
					}
				}
				break;
			case FIX_ALLIES ... FIX_COLORS: {
				int settingToSave = 1 << (clicked - FIX_ALLIES + SETTING_HOST_BALANCE_OFFSET);
				
				MENUITEMINFO info = {.cbSize = sizeof(MENUITEMINFO), .fMask = MIIM_STATE};
				GetMenuItemInfo(menu, clicked, 0, &info);
					
				gSettings.flags = (gSettings.flags & ~settingToSave) | !((info.fState & MFS_CHECKED)) * settingToSave;
				
				if (clicked <= FIX_CLANS)
					Rebalance();
				else
					FixPlayerStatus(NULL);
					
			}	break;
			case BALANCE_RANDOM: case BALANCE_RANK: case BALANCE_CLANS:
				SpadsMessageF("!set balanceMode %s",
				clicked == BALANCE_RANDOM ? "random"
				: clicked == BALANCE_RANK ? "rank"
				: "clan;rank");
				break;
			default:
				if (clicked & AI_FLAG) {
					char aiDll[128];
					GetMenuStringA(aiMenu, clicked, aiDll, sizeof(aiDll), MF_BYCOMMAND);
					static int lastBot;
					char botName[16];
					sprintf(botName, "bot%d", ++lastBot);
					uint32_t color = (rand() | rand() * (RAND_MAX + 1)) & 0xFFFFFF;
					if (gBattleOptions.hostType == HOST_SP)
						AddBot(botName, gMyBattle->founder, GetNewBattleStatus(), color, aiDll);
					else
						SendToServer("ADDBOT %s %d %d %s", botName, GetNewBattleStatus(), color, aiDll);
				} else if (clicked & TEAM_FLAG) {
					SetBattleStatus(gMyUser, FROM_TEAM_MASK(clicked & ~TEAM_FLAG), TEAM_MASK);
				} else if (clicked & MAP_FLAG) {
					char mapName[128];
					GetMenuStringA(mapMenu, clicked, mapName, sizeof(mapName), MF_BYCOMMAND);
					if (gBattleOptions.hostType == HOST_SPADS)
						SpadsMessageF("!map %s", mapName);
					else
						SendToServer("!UPDATEBATTLEINFO 0 0 %d %s", GetMapHash(mapName), mapName);
				} else if (clicked / SPLIT_FLAG)
					SetSplit((clicked / SPLIT_FLAG) - 1, clicked % SPLIT_FLAG);
				break;
			}

			DestroyMenu(menu);
			for (int i=0; i<lengthof(splitMenus); ++i)
				DestroyMenu(splitMenus[i]);
			break;
		}
		//These map/mod are only visible in sp mode:
		case MAKEWPARAM(DLG_MOD, CBN_SELCHANGE): {
			GetWindowTextA((HWND)lParam, gMyBattle->modName, sizeof(gMyBattle->modName));
			ChangeMod(gMyBattle->modName);
		}	return 0;
		case MAKEWPARAM(DLG_MAP, CBN_SELCHANGE): {
			GetWindowTextA((HWND)lParam, gMyBattle->mapName, sizeof(gMyBattle->mapName));
			ChangeMap(gMyBattle->mapName);
		}	return 0;
		case MAKEWPARAM(DLG_LEAVE, BN_CLICKED):
			goto close;
			return 0;
		case MAKEWPARAM(DLG_SPECTATE, BN_CLICKED):
			SetBattleStatus(gMyUser, ~gMyUser->battleStatus, MODE_MASK);
			return 0;
		case MAKEWPARAM(DLG_READY, BN_CLICKED):
			SetBattleStatus(gMyUser, ~gMyUser->battleStatus, READY_MASK);
			return 0;
		case MAKEWPARAM(DLG_ALLY, CBN_SELCHANGE):
			SetBattleStatus(gMyUser, TO_ALLY_MASK(SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0)), ALLY_MASK);
			SendMessage((HWND)lParam, CB_SETCURSEL, FROM_ALLY_MASK(gMyUser->battleStatus), 0);
			return 0;
		case MAKEWPARAM(DLG_SIDE, CBN_SELCHANGE):
			SetBattleStatus(gMyUser, TO_SIDE_MASK(SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0)), SIDE_MASK);
			SendMessage((HWND)lParam, CB_SETCURSEL, FROM_SIDE_MASK(gMyUser->battleStatus), 0);
			return 0;
		}
		break;
	case WM_RESYNC: {
		const char *def; int defIndex, i;
		
		HWND comboBox;
		void addToComboBox(const char *name)
		{
			if (def && !strcmp(def, name))
				defIndex = i;
			SendMessageA(comboBox, CB_ADDSTRING, 0, (LPARAM)name);
			++i;
		}
		def = LoadSetting("last_host_mod"); i=0; defIndex=0;
		comboBox = GetDlgItem(window, DLG_MOD);
		SendMessage(comboBox, CB_RESETCONTENT, 0, 0);
		ForEachModName(addToComboBox);
		SendDlgItemMessage(window, DLG_MOD, CB_SETCURSEL, defIndex, 0);
		
		def = LoadSetting("last_host_map"); i=0; defIndex=0;
		comboBox = GetDlgItem(window, DLG_MAP);
		SendMessage(comboBox, CB_RESETCONTENT, 0, 0);
		ForEachMapName(addToComboBox);
		SendDlgItemMessage(window, DLG_MAP, CB_SETCURSEL, defIndex, 0);
	 }	return 0;
	case WM_SETMODDETAILS: {
		HWND infoWindow = GetDlgItem(window, DLG_BATTLE_INFO);
		POINT scrollPosition;
		SendMessage(infoWindow, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPosition);
		SendMessage(infoWindow, EM_SETTEXTEX, (WPARAM)&(SETTEXTEX){.codepage = 65001}, lParam);
		free((void *)lParam);
		for (int startPos, endPos=0; (startPos = SendMessage(infoWindow, EM_FINDTEXT, FR_DOWN, (LPARAM)&(FINDTEXT){{endPos,-1}, L":<"})) >= 0; ) {
			endPos = SendMessage(infoWindow, EM_FINDTEXT, FR_DOWN, (LPARAM)&(FINDTEXT){{startPos, -1}, L">:"});
			SendMessage(infoWindow, EM_SETSEL, startPos, endPos);
			SendMessage(infoWindow, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&(CHARFORMAT2){.cbSize = sizeof(CHARFORMAT2), .dwMask = CFM_LINK, .dwEffects = CFE_LINK});
		}
		SendMessage(infoWindow, EM_SETSEL, 0, 0);
		SendMessage(infoWindow, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPosition);
		} return 0;
	case WM_CHANGEMOD: {
		HWND sideBox = GetDlgItem(window, DLG_SIDE);
		SendDlgItemMessage(sideBox, DLG_SIDE, CB_RESETCONTENT, 0, 0);

		for (int i=0; *gSideNames[i]; ++i)
			SendMessage(sideBox, CBEM_INSERTITEMA, 0,
					(LPARAM)&(COMBOBOXEXITEMA){
						.iItem = -1, .mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE,
							.pszText = gSideNames[i],
						.iImage = ICONS_FIRST_SIDE + i, .iSelectedImage = ICONS_FIRST_SIDE + i,
					});
		HWND playerList = GetDlgItem(window, DLG_PLAYER_LIST);
		LVITEM item = {LVIF_PARAM};
		for (item.iItem= -1; (item.iItem = SendMessage(playerList, LVM_GETNEXTITEM, item.iItem, 0)) >= 0;) {
			item.mask = LVIF_PARAM;
			SendMessage(playerList, LVM_GETITEM, 0, (LPARAM)&item);
			item.mask = LVIF_IMAGE;
			item.iSubItem = COLUMN_SIDE;
			union UserOrBot *s = (void *)item.lParam;
			item.iImage = s->battleStatus & MODE_MASK && *gSideNames[FROM_SIDE_MASK(s->battleStatus)] ? ICONS_FIRST_SIDE + FROM_SIDE_MASK(s->battleStatus) : -1;
			SendMessage(playerList, LVM_SETITEM, 0, (LPARAM)&item);
		}
		SendMessage(sideBox, CB_SETCURSEL, gMyUser ? FROM_SIDE_MASK(gMyUser->battleStatus) : 0, 0);
		// resizePlayerListTabs();
		if (gBattleOptions.startPosType == STARTPOS_CHOOSE_INGAME)
			for (int i=0; i<16; ++i)
				SetWindowPos(GetDlgItem(GetDlgItem(gBattleRoomWindow, DLG_MINIMAP), DLG_FIRST_STARTPOS + i), NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW);
		} break;
	case WM_REDRAWMINIMAP: {
		HWND minimap = GetDlgItem(window, DLG_MINIMAP);
		HBITMAP bitmap = (void *)SendMessage(minimap, STM_SETIMAGE, IMAGE_BITMAP, lParam);
		DeleteObject(bitmap);
		return 0;
	case WM_MOVESTARTPOSITIONS:
		minimap = GetDlgItem(window, DLG_MINIMAP);
		for (int i=0; i<16; ++i) {
			SetWindowPos(GetDlgItem(GetDlgItem(gBattleRoomWindow, DLG_MINIMAP), DLG_FIRST_STARTPOS + i), NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW);
		}
		if (gBattleOptions.startPosType == STARTPOS_CHOOSE_INGAME
				|| !gMapInfo.width || !gMapInfo.height)
			return 0;
		RECT rect;
		GetClientRect(minimap, &rect);
		int mapWidth = gMapInfo.width, mapHeight = gMapInfo.height;
		
		if (!gMyBattle)
			return 0;
		int startBoxWidth = MAP_X(14);
		FOR_EACH_PLAYER(s, gMyBattle) {
			int i = FROM_TEAM_MASK(s->battleStatus);
			StartPos pos = GET_STARTPOS(i);
			HWND startPosControl = GetDlgItem(minimap, DLG_FIRST_STARTPOS + i);
			SetWindowLongPtr(startPosControl, GWLP_USERDATA, (LONG_PTR)s);
			SetWindowPos(startPosControl, NULL,
					rect.right * pos.x / mapWidth - startBoxWidth/2, rect.bottom * pos.z / mapHeight - startBoxWidth/2, startBoxWidth, startBoxWidth, SWP_SHOWWINDOW);
			SendMessage(startPosControl, BM_SETIMAGE, IMAGE_ICON, (WPARAM)ImageList_GetIcon(iconList, GetColorIndex(s), 0));
		}
		} return 0;
	}
	return DefWindowProc(window, msg, wParam, lParam);
}


static void __attribute__((constructor)) _init_ (void)
{
	RegisterClassEx((&(WNDCLASSEX){
		.lpszClassName = WC_BATTLEROOM,
		.cbSize        = sizeof(WNDCLASSEX),
		.lpfnWndProc   = battleRoomProc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	}));
}

void CreateSinglePlayerDlg(void)
{
	ResetBattleOptions();
	gBattleOptions.hostType = HOST_SP;
	gBattleOptions.startPosType = STARTPOS_CHOOSE_BEFORE;
	
	gMyBattle = calloc(1, sizeof(Battle));
	GetDlgItemTextA(gBattleRoomWindow, DLG_MOD, gMyBattle->modName, sizeof(gMyBattle->modName));
	GetDlgItemTextA(gBattleRoomWindow, DLG_MAP, gMyBattle->mapName, sizeof(gMyBattle->mapName));
	
	// if (!gModHash || gModHash != GetModHash(gMyBattle->modName))
	ChangeMod(gMyBattle->modName);
	if (!gMapHash || gMapHash != GetMapHash(gMyBattle->mapName))
		ChangeMap(gMyBattle->mapName);
	gMyBattle->mapHash = gMapHash;
	gBattleOptions.modHash = gModHash;
	
	gMyUser->battleStatus = READY_MASK | MODE_MASK | SYNC_MASK;
	gMyUser->battle = gMyBattle;
	SendDlgItemMessage(gBattleRoomWindow, DLG_SPECTATE, BM_SETCHECK, 0, 0);
	gMyBattle->founder = gMyUser;
	gMyBattle->nbParticipants = 1;

	BattleRoom_Show();
}
