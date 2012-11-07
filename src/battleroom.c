#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <malloc.h>
#include <stdio.h>

#include <windows.h>
#include <windowsx.h>
#include <Commctrl.h>
#include <richedit.h>
#include <commdlg.h>

#include "alphalobby.h"
#include "battleroom.h"
#include "battletools.h"
#include "chat.h"
#include "chat_window.h"
#include "client_message.h"
#include "countrycodes.h"
#include "data.h"
#include "imagelist.h"
#include "layoutmetrics.h"
#include "listview.h"
#include "settings.h"
#include "spring.h"
#include "sync.h"
#include "usermenu.h"
#include "wincommon.h"

HWND gBattleRoom;

#define LENGTH(x) (sizeof(x) / sizeof(*x))

#define CFE_LINK		0x0020
// #define CFM_LINK		0x00000020
#define CFM_STYLE			0x00080000
#define CFM_KERNING			0x00100000

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
	DLG_PLAYER_LIST,
	DLG_ALLY,
	DLG_SIDE_LABEL,
	DLG_SIDE_FIRST,
	DLG_SIDE_LAST = DLG_SIDE_FIRST+4,
	DLG_SPECTATE,
	DLG_AUTO_UNSPEC,
	DLG_MINIMAP,
	DLG_START,
	DLG_LEAVE,
	DLG_BATTLE_INFO,


	DLG_SPLIT_FIRST,
	DLG_SPLIT_LAST = DLG_SPLIT_FIRST + SPLIT_LAST,
	
	DLG_SPLIT_SIZE,
	
	DLG_MAPMODE_LABEL,
	DLG_MAPMODE_MINIMAP,
	DLG_MAPMODE_METAL,
	DLG_MAPMODE_ELEVATION,
	DLG_CHANGE_MAP,
	
	DLG_VOTEBOX,
	DLG_VOTETEXT,
	DLG_VOTEYES,
	DLG_VOTENO,
	
	//Single Player Only:
	DLG_MOD,
	DLG_MAP,
	
	
	DLG_LAST = DLG_MAP,
	DLG_STARTPOS,
	DLG_FIRST_STARTPOS = DLG_STARTPOS,
};

static const DialogItem dialogItems[] = {
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
	}, [DLG_MOD] = {
		.class = WC_COMBOBOX,
		.style = WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
	}, [DLG_MAP] = {
		.class = WC_COMBOBOX,
		.style = WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
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
	}, [DLG_PLAYER_LIST] = {
		.class = WC_LISTVIEW,
		.exStyle = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | LVS_SHAREIMAGELISTS | LVS_SINGLESEL | LVS_REPORT | LVS_NOCOLUMNHEADER,
	}, [DLG_BATTLE_INFO] = {
		.class = WC_LISTVIEW,
		.style = WS_VISIBLE | LVS_SINGLESEL | LVS_REPORT,

	}, [DLG_VOTEBOX] = {
		.class = WC_BUTTON,
		.name = L"Poll",
		.style = WS_VISIBLE | BS_GROUPBOX,
	}, [DLG_VOTETEXT] = {
		.class = WC_STATIC,
		.style = WS_VISIBLE | WS_DISABLED,
	}, [DLG_VOTEYES] = {
		.class = WC_BUTTON,
		.name = L"Yes",
		.style = WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
	}, [DLG_VOTENO] = {
		.class = WC_BUTTON,
		.name = L"No",
		.style = WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
	
	}, [DLG_MAPMODE_LABEL] = {
		.class = WC_STATIC,
		.name = L"Map:",
		.style = WS_VISIBLE,
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
	}, [DLG_STARTPOS] = {
		.class = WC_BUTTON,
		.style = WS_VISIBLE | BS_PUSHBUTTON | BS_ICON,
	}
};

void BattleRoom_ChangeMinimapBitmap(const uint16_t *_minimapPixels,
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
	
	InvalidateRect(GetDlgItem(gBattleRoom, DLG_MINIMAP), 0, 0);
}

void BattleRoom_RedrawMinimap(void)
{
	InvalidateRect(GetDlgItem(gBattleRoom, DLG_MINIMAP), 0, 0);
}

void BattleRoom_StartPositionsChanged(void)
{
	int size;
	SplitType splitType = -1;
	
	
	if (gBattleOptions.startPosType == STARTPOS_CHOOSE_INGAME) {
		if (!memcmp(&gBattleOptions.startRects[0], &(RECT){0, 0, 200 - gBattleOptions.startRects[1].left, 200}, sizeof(RECT))) {
			splitType = SPLIT_VERT;
			size = gBattleOptions.startRects[0].right;
		} else if (!memcmp(&gBattleOptions.startRects[0], &(RECT){0, 0, 200, 200 - gBattleOptions.startRects[1].top}, sizeof(RECT))) {
			splitType = SPLIT_HORZ;
			size = gBattleOptions.startRects[0].bottom;
		} else if (!memcmp(&gBattleOptions.startRects[0], &(RECT){0, 0, 200 - gBattleOptions.startRects[1].left, 200 - gBattleOptions.startRects[1].top}, sizeof(RECT))) {
			splitType = SPLIT_CORNERS1;
			size = gBattleOptions.startRects[0].right;
		} else if (!memcmp(&gBattleOptions.startRects[0], &(RECT){0, 200 - gBattleOptions.startRects[1].bottom, 200 - gBattleOptions.startRects[1].left, 200}, sizeof(RECT))) {
			splitType = SPLIT_CORNERS2;
			size = gBattleOptions.startRects[0].right;
		}
	}

	for (int i=0; i<=SPLIT_LAST; ++i)
		SendDlgItemMessage(gBattleRoom, DLG_SPLIT_FIRST + i, BM_SETCHECK, i == splitType ? BST_CHECKED : BST_UNCHECKED, 0);
	
	SendDlgItemMessage(gBattleRoom, DLG_SPLIT_FIRST + SPLIT_RAND, BM_SETCHECK, gBattleOptions.startPosType != STARTPOS_CHOOSE_INGAME ? BST_CHECKED : BST_UNCHECKED, 0);
	EnableWindow(GetDlgItem(gBattleRoom, DLG_SPLIT_SIZE), gBattleOptions.startPosType == STARTPOS_CHOOSE_INGAME);
	if (splitType >= 0)
		SendDlgItemMessage(gBattleRoom, DLG_SPLIT_SIZE, TBM_SETPOS, 1, size);
	InvalidateRect(GetDlgItem(gBattleRoom, DLG_MINIMAP), 0, 0);
}
#if 0
	case WM_MOVESTARTPOSITIONS:
		HWND minimap = GetDlgItem(window, DLG_MINIMAP);
		for (int i=0; i<16; ++i) {
			SetWindowPos(GetDlgItem(GetDlgItem(gBattleRoom, DLG_MINIMAP), DLG_FIRST_STARTPOS + i), NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW);
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
			SendMessage(startPosControl, BM_SETIMAGE, IMAGE_ICON, (WPARAM)ImageList_GetIcon(gIconList, GetColorIndex(s), 0));
		}
		} return 0;
#endif

void BattleRoom_Show(void)
{
	int isSP = gBattleOptions.hostType == HOST_SP;
	ShowWindow(GetDlgItem(gBattleRoom, DLG_CHAT), !isSP);
	ShowWindow(GetDlgItem(gBattleRoom, DLG_PLAYER_LIST), !isSP);
	ShowWindow(GetDlgItem(gBattleRoom, DLG_ALLY), !isSP);
	ShowWindow(GetDlgItem(gBattleRoom, DLG_MAP), isSP);
	ShowWindow(GetDlgItem(gBattleRoom, DLG_MOD), isSP);
	if (isSP)
		ShowWindow(GetDlgItem(gBattleRoom, DLG_MINIMAP), 1);
	EnableWindow(GetDlgItem(gBattleRoom, DLG_START), isSP);
	RECT rect;
	GetClientRect(gBattleRoom, &rect);
	SendMessage(gBattleRoom, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
	MainWindow_EnableBattleroomButton();
}

void BattleRoom_Hide(void)
{
	ListView_DeleteAllItems(GetDlgItem(gBattleRoom, DLG_PLAYER_LIST));
	MainWindow_DisableBattleroomButton();
}

static int findUser(const void *u)
{
	return SendDlgItemMessage(gBattleRoom, DLG_PLAYER_LIST, LVM_FINDITEM, -1,
			(LPARAM)&(LVFINDINFO){.flags = LVFI_PARAM, .lParam = (LPARAM)u});
}

static void *getUserFromIndex(int index)
{
	LVITEM item = {
		.mask = LVIF_PARAM,
		.iItem = index,
	};
	SendDlgItemMessage(gBattleRoom, DLG_PLAYER_LIST, LVM_GETITEM, 0, (LPARAM)&item);
	return (void *)item.lParam;
}

void BattleRoom_RemoveUser(const union UserOrBot *s)
{
	SendDlgItemMessage(gBattleRoom, DLG_PLAYER_LIST, LVM_DELETEITEM, findUser(s), 0);
}

static void setIcon(int item, int subItem, enum ICONS icon) {
	SendDlgItemMessage(gBattleRoom, DLG_PLAYER_LIST, LVM_SETITEM, 0, (LPARAM)&(LVITEM){.iItem = item, .iSubItem = subItem, .mask = LVIF_IMAGE, .iImage = icon});
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

static void updatePlayerListGroup(int groupId)
{
	if (groupId >= 16)
		return;

	wchar_t buff[256];
	int playersOnTeam = 0;
	FOR_EACH_PLAYER(p, gMyBattle)
		playersOnTeam += FROM_ALLY_MASK(p->battleStatus) == groupId;
	swprintf(buff, L"Team %d :: %d Player%c", groupId + 1, playersOnTeam, playersOnTeam > 1 ? 's' : '\0');
	
	SendDlgItemMessage(gBattleRoom, DLG_PLAYER_LIST, LVM_SETGROUPINFO, groupId,
		(LPARAM)&(LVGROUP){
			.cbSize = sizeof(LVGROUP),
			.mask = LVGF_HEADER,
			.pszHeader = buff,
		});
}

char BattleRoom_IsAutoUnspec(void)
{
	return SendDlgItemMessage(gBattleRoom, DLG_AUTO_UNSPEC, BM_GETCHECK, 0, 0);
}

void BattleRoom_UpdateUser(union UserOrBot *s)
{
	HWND playerList = GetDlgItem(gBattleRoom, DLG_PLAYER_LIST);

	uint32_t battleStatus = s->battleStatus;
	int groupId = (battleStatus & MODE_MASK) ? FROM_ALLY_MASK(battleStatus) : 16;

	int item = findUser(s);
	if (item == -1) {
		item = SendMessage(playerList, LVM_INSERTITEM, 0,
			(LPARAM)&(LVITEM){
				.mask = LVIF_PARAM | LVIF_GROUPID,
				.lParam = (LPARAM)s,
				.iGroupId = groupId
			});
	} else {
		LVITEM info = {.iItem = item, .mask = LVIF_GROUPID};
		ListView_GetItem(playerList, &info);
		updatePlayerListGroup(info.iGroupId);
		info.iGroupId = groupId;
		ListView_SetItem(playerList, &info);
	}
	updatePlayerListGroup(groupId);
	
	
	SendMessage(playerList, LVM_SETITEM, 0, (LPARAM)&(LVITEM){
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
		SendMessage(playerList, LVM_SETITEM, 0, (LPARAM)&(LVITEM){
				LVIF_TEXT, item, COLUMN_NAME,
				.pszText = name});
		goto sort;
	}
	
	User *u = &s->user;
	
	setIcon(item, COLUMN_FLAG, ICONS_FIRST_FLAG + u->country);
	setIcon(item, COLUMN_RANK, ICONS_FIRST_RANK + FROM_RANK_MASK(u->clientStatus));

	if (u == &gMyUser) {
		SendDlgItemMessage(gBattleRoom, DLG_SPECTATE, BM_SETCHECK, !(battleStatus & MODE_MASK), 0);
		EnableWindow(GetDlgItem(gBattleRoom, DLG_AUTO_UNSPEC), !(battleStatus & MODE_MASK));
		if (battleStatus & MODE_MASK)
			SendDlgItemMessage(gBattleRoom, DLG_AUTO_UNSPEC, BM_SETCHECK, BST_UNCHECKED, 0);
		
		for (int i=DLG_SIDE_FIRST; i<=DLG_SIDE_LAST; ++i)
			SendDlgItemMessage(gBattleRoom, i, BM_SETCHECK, FROM_SIDE_MASK(battleStatus) == i - DLG_SIDE_FIRST ? BST_CHECKED : BST_UNCHECKED, 0);
			
		SendDlgItemMessage(gBattleRoom, DLG_ALLY, CB_SETCURSEL, FROM_ALLY_MASK(battleStatus), 0);
	}
	if (u->battle->founder == u)
		EnableWindow(GetDlgItem(gBattleRoom, DLG_START), !!(u->clientStatus & CS_INGAME_MASK) ^ (u == &gMyUser) || gBattleOptions.hostType & HOST_FLAG);

	int teamSizes[16] = {};
	FOR_EACH_PLAYER(u, gMyBattle)
		++teamSizes[FROM_TEAM_MASK(u->battleStatus)];
	FOR_EACH_USER(u, gMyBattle) {
		wchar_t buff[128], *s=buff;
		if ((u->battleStatus & MODE_MASK) && teamSizes[FROM_TEAM_MASK(u->battleStatus)] > 1)
			s += swprintf(s, L"%d: ", FROM_TEAM_MASK(u->battleStatus)+1);
		s += swprintf(s, L"%hs", u->name);
		if (strcmp(GetAliasOf(u->name), u->alias))
			s += swprintf(s, L" (%hs)", u->alias);
		SendMessage(playerList, LVM_SETITEM, 0, (LPARAM)&(LVITEM){
				.mask = LVIF_TEXT,
				.iItem = findUser(u),
				.iSubItem = COLUMN_NAME,
				.pszText = buff});
	}
	sort:
	SendMessage(playerList, LVM_SORTITEMS, 0, (LPARAM)sortPlayerList);
}

void resizePlayerListTabs(void)
{
	HWND playerList = GetDlgItem(gBattleRoom, DLG_PLAYER_LIST);
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
		(DeferWindowPos(dwp, (GetDlgItem(gBattleRoom, (id))), NULL, (x), (y), (cx), (cy), 0))
	
	
	MOVE_ID(DLG_BATTLE_INFO, XS, YS, INFO_WIDTH, INFO_HEIGHT);
	MOVE_ID(DLG_PLAYER_LIST, INFO_WIDTH + 2*XS, YS, LIST_WIDTH, INFO_HEIGHT);
	int minimapX = INFO_WIDTH + LIST_WIDTH + 3*XS;
	MOVE_ID(DLG_MINIMAP, minimapX, MAP_Y(14 + 2*S), width - minimapX - XS, INFO_HEIGHT - MAP_Y(28 + 4*S));
	MOVE_ID(DLG_CHAT, XS, CHAT_TOP, CHAT_WIDTH - MAP_X(7), h - INFO_HEIGHT - 3*YS);
	
	MOVE_ID(DLG_START, CHAT_WIDTH, h - MAP_Y(14 + S), MAP_X(50), MAP_Y(14));
	MOVE_ID(DLG_LEAVE, CHAT_WIDTH + MAP_X(54), h - MAP_Y(14 + S), MAP_X(50), MAP_Y(14));

	#define VOTEBOX_TOP (CHAT_TOP)
	#define VOTETEXT_HEIGHT (2 * COMMANDBUTTON_Y)
	#define VOTETEXT_WIDTH (MAP_X(2 * 50 + 4))
	#define VOTEBUTTON_WIDTH ((VOTETEXT_WIDTH - MAP_X(14)) / 2)
	MOVE_ID(DLG_VOTEBOX,	CHAT_WIDTH, VOTEBOX_TOP, VOTETEXT_WIDTH, VOTETEXT_HEIGHT + MAP_Y(3*7 + 11));
	MOVE_ID(DLG_VOTETEXT,	CHAT_WIDTH + MAP_X(6), VOTEBOX_TOP + MAP_Y(11), MAP_X(2 * 50), VOTETEXT_HEIGHT);
	MOVE_ID(DLG_VOTEYES,	CHAT_WIDTH + MAP_X(6), VOTEBOX_TOP + VOTETEXT_HEIGHT + MAP_Y(13), VOTEBUTTON_WIDTH, COMMANDBUTTON_Y);
	MOVE_ID(DLG_VOTENO,		CHAT_WIDTH + MAP_X(10) + VOTEBUTTON_WIDTH, VOTEBOX_TOP + VOTETEXT_HEIGHT + MAP_Y(13), VOTEBUTTON_WIDTH, COMMANDBUTTON_Y);

	#define VOTEBOX_BOTTOM (VOTEBOX_TOP + VOTETEXT_HEIGHT + MAP_Y(41))
	MOVE_ID(DLG_ALLY_LABEL,  CHAT_WIDTH, VOTEBOX_BOTTOM, MAP_X(70), TEXTLABEL_Y);
	MOVE_ID(DLG_ALLY,        CHAT_WIDTH, VOTEBOX_BOTTOM + MAP_Y(10), MAP_X(70), COMMANDBUTTON_Y);
	
	MOVE_ID(DLG_SIDE_LABEL,  CHAT_WIDTH, VOTEBOX_BOTTOM + MAP_Y(31), MAP_X(70), TEXTLABEL_Y);

	for (int i=0; i <= DLG_SIDE_LAST - DLG_SIDE_FIRST; ++i)
		MOVE_ID(DLG_SIDE_FIRST + i, CHAT_WIDTH + i * (COMMANDBUTTON_Y + MAP_X(4)), VOTEBOX_BOTTOM + MAP_Y(41), COMMANDBUTTON_Y, COMMANDBUTTON_Y);

	MOVE_ID(DLG_SPECTATE,    CHAT_WIDTH, VOTEBOX_BOTTOM + MAP_Y(62), MAP_X(70), TEXTBOX_Y);
	MOVE_ID(DLG_AUTO_UNSPEC, CHAT_WIDTH + MAP_X(10), VOTEBOX_BOTTOM + MAP_Y(77), MAP_X(80), TEXTBOX_Y);

	for (int i=0; i<=SPLIT_LAST; ++i)
		MOVE_ID(DLG_SPLIT_FIRST + i, minimapX + (1 + i) * XS + i * YH, YS, MAP_Y(14), MAP_Y(14));
	MOVE_ID(DLG_SPLIT_SIZE, minimapX + 6*XS + 5*YH, YS, width - minimapX - 7*XS - 5*YH, YH);
	
	#define TOP (INFO_HEIGHT - MAP_Y(14 + S))
	MOVE_ID(DLG_MAPMODE_LABEL,     minimapX + XS, TOP + MAP_Y(3),   MAP_X(20), TEXTBOX_Y);
	MOVE_ID(DLG_MAPMODE_MINIMAP,   minimapX + XS + MAP_X(20),  TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_MAPMODE_METAL,     minimapX + XS + MAP_X(70),  TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_MAPMODE_ELEVATION, minimapX + XS + MAP_X(120), TOP, MAP_X(50), COMMANDBUTTON_Y);
	MOVE_ID(DLG_CHANGE_MAP,        width - 2*XS - MAP_X(60),   TOP, MAP_X(60), COMMANDBUTTON_Y);
	#undef TOP
	
	EndDeferWindowPos(dwp);
	resizePlayerListTabs();
	BattleRoom_StartPositionsChanged();
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
			BattleRoom_StartPositionsChanged();
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

static void onCreate(HWND window)
{
	gBattleRoom = window;
	CreateDlgItems(window, dialogItems, DLG_LAST + 1);
	for (int i=0; i<16; ++i) {
		HWND minimap = GetDlgItem(window, DLG_MINIMAP);
		HWND startPos = CreateDlgItem(minimap, &dialogItems[DLG_STARTPOS], DLG_FIRST_STARTPOS + i);
		SetWindowSubclass(startPos, startPositionProc, i, i);
	}
	for (int i=0; i<16; ++i) {
		wchar_t buff[LENGTH("Team 16")];
		swprintf(buff, L"Team %d", i+1);
		SendDlgItemMessage(window, DLG_ALLY, CB_ADDSTRING, 0, (LPARAM)buff);
	}

	HWND infoList = GetDlgItem(window, DLG_BATTLE_INFO);
	SendMessage(infoList, LVM_INSERTCOLUMN, 0, (LPARAM)&(LVCOLUMN){});
	SendMessage(infoList, LVM_INSERTCOLUMN, 1, (LPARAM)&(LVCOLUMN){});
	ListView_EnableGroupView(infoList, TRUE);

	HWND playerList = GetDlgItem(window, DLG_PLAYER_LIST);
	for (int i=0; i <= COLUMN_LAST; ++i)
		SendMessage(playerList, LVM_INSERTCOLUMN, i, (LPARAM)&(LVCOLUMN){});

	ListView_SetExtendedListViewStyle(playerList, LVS_EX_DOUBLEBUFFER |  LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
	SendMessage(playerList, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)gIconList);
	ListView_EnableGroupView(playerList, TRUE);
	for (int i=0; i<=16; ++i) {
		wchar_t buff[LENGTH("Spectators")];
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

	SendDlgItemMessage(window, DLG_SPLIT_SIZE, TBM_SETRANGE, 1,
			MAKELONG(0, 200));
	SendDlgItemMessage(window, DLG_SPLIT_FIRST + SPLIT_VERT,
			BM_SETIMAGE, IMAGE_ICON,
			(WPARAM)ImageList_GetIcon(gIconList, ICONS_SPLIT_VERT, 0));
	SendDlgItemMessage(window, DLG_SPLIT_FIRST + SPLIT_HORZ,
			BM_SETIMAGE, IMAGE_ICON,
			(WPARAM)ImageList_GetIcon(gIconList, ICONS_SPLIT_HORZ, 0));
	SendDlgItemMessage(window, DLG_SPLIT_FIRST + SPLIT_CORNERS1,
			BM_SETIMAGE, IMAGE_ICON,
			(WPARAM)ImageList_GetIcon(gIconList, ICONS_SPLIT_CORNER1, 0));
	SendDlgItemMessage(window, DLG_SPLIT_FIRST + SPLIT_CORNERS2,
			BM_SETIMAGE, IMAGE_ICON,
			(WPARAM)ImageList_GetIcon(gIconList, ICONS_SPLIT_CORNER2, 0));
	SendDlgItemMessage(window, DLG_SPLIT_FIRST + SPLIT_RAND,
			BM_SETIMAGE, IMAGE_ICON,
			(WPARAM)ImageList_GetIcon(gIconList, ICONS_SPLIT_RAND, 0));

	SendDlgItemMessage(window, DLG_MAPMODE_MINIMAP, BM_SETCHECK, BST_CHECKED, 0);
}

static void onDraw(DRAWITEMSTRUCT *info)
{
	FillRect(info->hDC, &info->rcItem, (HBRUSH) (COLOR_BTNFACE+1));

	if (!minimapPixels) {
		/* extern void GetDownloadMessage(const char *text); */
		/* char text[256]; */
		/* GetDownloadMessage(text); */
		/* DrawTextA(info->hDC, text, -1, &info->rcItem, DT_CENTER); */
		return;
	}

	int width = info->rcItem.right;
	int height = info->rcItem.bottom;

	if (!gMapInfo.width || !gMapInfo.height || !width || !height)
		return;

	if (height * gMapInfo.width > width * gMapInfo.height)
		height = width * gMapInfo.height / gMapInfo.width;
	else
		width = height * gMapInfo.width / gMapInfo.height;

	int xOffset = (info->rcItem.right - width) / 2;
	int yOffset = (info->rcItem.bottom - height) / 2;


	uint32_t *pixels = malloc(width * height * 4);
	if (SendDlgItemMessage(gBattleRoom, DLG_MAPMODE_ELEVATION, BM_GETCHECK, 0, 0)) {
		for (int i=0; i<width * height; ++i) {
			uint8_t heightPixel = heightMapPixels[i % width * heightMapWidth / width + i / width * heightMapHeight / height * heightMapWidth];
			pixels[i] = heightPixel | heightPixel << 8 | heightPixel << 16;
		}
	} else if (SendDlgItemMessage(gBattleRoom, DLG_MAPMODE_METAL, BM_GETCHECK, 0, 0)) {
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

	if (gBattleOptions.startPosType == STARTPOS_CHOOSE_INGAME) {
		for (int j=0; j<NUM_ALLIANCES; ++j) {
			int xMin = gBattleOptions.startRects[j].left * width / START_RECT_MAX;
			int xMax = gBattleOptions.startRects[j].right * width / START_RECT_MAX;
			int yMin = gBattleOptions.startRects[j].top * height / START_RECT_MAX;
			int yMax = gBattleOptions.startRects[j].bottom * height / START_RECT_MAX;

			if ((gMyUser.battleStatus & MODE_MASK) && j == FROM_ALLY_MASK(gMyUser.battleStatus)) {
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
				// pixels[x+width*(yMin+1)] = 0;
				pixels[x+width*(yMax-1)] = 0;
				// pixels[x+width*(yMax-2)] = 0;
			}
			for (int y=yMin; y<yMax; ++y) {
				pixels[xMin+width*y] = 0;
				// pixels[(xMin+1)+width*y] = 0;
				pixels[(xMax-1)+width*y] = 0;
				// pixels[(xMax-2)+width*y] = 0;
			}
		}
	}
	HBITMAP bitmap = CreateBitmap(width, height, 1, 32, pixels);
	free(pixels);

	HDC dcSrc = CreateCompatibleDC(info->hDC);
	SelectObject(dcSrc, bitmap);
	BitBlt(info->hDC, xOffset, yOffset, width, height, dcSrc, 0, 0, SRCCOPY);

	DeleteObject(bitmap);
	// printf("is it? %d\n", ReleaseDC(info->hwndItem, dcSrc));
}

static LRESULT onNotify(WPARAM wParam, NMHDR *note)
{
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
		if (strcmp(GetAliasOf(u->name), u->alias))
			s += swprintf(s, L" (%hs)", u->alias);

		if (!(u->battleStatus & AI_MASK))
			s += swprintf(s, L"\nRank %d - %hs - %.2fGHz\n",
					FROM_RANK_MASK(u->clientStatus),
					countryNames[u->country],
					(float)u->cpu / 1000);

		if (battleStatus & MODE_MASK) {
			const char *sideName = gSideNames[FROM_SIDE_MASK(battleStatus)];

			s += swprintf(s, L"Player %d - Team %d",
					FROM_TEAM_MASK(battleStatus),
					FROM_ALLY_MASK(battleStatus));
			if (*sideName)
				s += swprintf(s, L" - %hs", sideName);

			if (battleStatus & HANDICAP_MASK)
				s += swprintf(s, L"\nHandicap: %d",
						FROM_HANDICAP_MASK(battleStatus));
		} else {
			s += swprintf(s, L"Spectator");
		}

		((NMTTDISPINFO *)note)->lpszText = buff;
		return 0;
	}
	switch (note->idFrom) {
		/* case DLG_BATTLE_INFO: { */
		/* ENLINK *link = (ENLINK *)note; */
		/* if (note->code != EN_LINK || link->msg != WM_LBUTTONUP) */
		/* break; */
		/* SendMessage(note->hwndFrom, EM_EXSETSEL, 0, (LPARAM)&link->chrg); */
		/* wchar_t buff[link->chrg.cpMax - link->chrg.cpMin]; */
		/* SendMessage(note->hwndFrom, EM_GETSELTEXT, 0, (LPARAM)buff); */

		/* int i = _wtoi(buff + LENGTH(L":<") - 1); */
		/* if (i & MOD_OPTION_FLAG) */
		/* ChangeModOption(i & ~MOD_OPTION_FLAG); */
		/* else */
		/* ChangeMapOption(i); */
		/* }	return 0; */
	case DLG_PLAYER_LIST:
		switch (note->code) {
			POINT pt;
			int index;
		case LVN_ITEMACTIVATE:
			ChatWindow_SetActiveTab(GetPrivateChat(getUserFromIndex(((LPNMITEMACTIVATE)note)->iItem)));
			return 1;
		case NM_RCLICK:
			pt =  ((LPNMITEMACTIVATE)note)->ptAction;
			index = SendMessage(note->hwndFrom, LVM_SUBITEMHITTEST, 0, (LPARAM)&(LVHITTESTINFO){.pt = pt});
			if (index < 0)
				return 0;

			CreateUserMenu(getUserFromIndex(index), gBattleRoom);
			return 1;
		}
		break;
	}
	return DefWindowProc(gBattleRoom, WM_NOTIFY, wParam, (LPARAM)note);
}

static LRESULT onCommand(WPARAM wParam, HWND window)
{
	HMENU menu;
	char voteYes = 0;

	switch (wParam) {
	case MAKEWPARAM(DLG_START, BN_CLICKED):
		LaunchSpring();
		return 0;

	case MAKEWPARAM(DLG_CHANGE_MAP, BN_CLICKED): 
		menu = CreatePopupMenu();
		for (int i=0; i<gNbMaps; ++i)
			AppendMenuA(menu, MF_CHECKED * !strcmp(gMyBattle->mapName,  gMaps[i]), i + 1, gMaps[i]);
		POINT pt;
		GetCursorPos(&pt);
		int mapIndex = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y, gBattleRoom, NULL);
		if (mapIndex > 0)
			ChangeMap(gMaps[mapIndex - 1]);
		DestroyMenu(menu);
		return 0;

	case MAKEWPARAM(DLG_SPLIT_FIRST, BN_CLICKED) ... MAKEWPARAM(DLG_SPLIT_LAST, BN_CLICKED):
		SetSplit(LOWORD(wParam) - DLG_SPLIT_FIRST, SendDlgItemMessage(gBattleRoom, DLG_SPLIT_SIZE, TBM_GETPOS, 0, 0));
		return 0;
		//These map/mod are only visible in sp mode:

	case MAKEWPARAM(DLG_MOD, CBN_SELCHANGE): 
		GetWindowTextA(window, gMyBattle->modName, sizeof(gMyBattle->modName));
		ChangedMod(gMyBattle->modName);
		return 0;

	case MAKEWPARAM(DLG_MAP, CBN_SELCHANGE): 
		GetWindowTextA(window, gMyBattle->mapName, sizeof(gMyBattle->mapName));
		ChangedMap(gMyBattle->mapName);
		return 0;

	case MAKEWPARAM(DLG_LEAVE, BN_CLICKED):
		SendMessage(gBattleRoom, WM_CLOSE, 0, 0);
		return 0;

	case MAKEWPARAM(DLG_SPECTATE, BN_CLICKED):
		SetBattleStatus(&gMyUser, ~gMyUser.battleStatus, MODE_MASK);
		return 0;

	case MAKEWPARAM(DLG_ALLY, CBN_SELCHANGE):
		SetBattleStatus(&gMyUser, TO_ALLY_MASK(SendMessage(window, CB_GETCURSEL, 0, 0)), ALLY_MASK);
		SendMessage(window, CB_SETCURSEL, FROM_ALLY_MASK(gMyUser.battleStatus), 0);
		return 0;

	case MAKEWPARAM(DLG_SIDE_FIRST, BN_CLICKED) ... MAKEWPARAM(DLG_SIDE_LAST, BN_CLICKED):
		SetBattleStatus(&gMyUser, TO_SIDE_MASK(LOWORD(wParam) - DLG_SIDE_FIRST), SIDE_MASK);
		return 0;

	case MAKEWPARAM(DLG_MAPMODE_MINIMAP, BN_CLICKED) ... MAKEWPARAM(DLG_MAPMODE_ELEVATION, BN_CLICKED):
		InvalidateRect(GetDlgItem(gBattleRoom, DLG_MINIMAP), 0, 0);
		return 0;

	case MAKEWPARAM(DLG_VOTEYES, BN_CLICKED): 
		voteYes = 1;
		/* Fallthrough: */

	case MAKEWPARAM(DLG_VOTENO, BN_CLICKED):
		if (gBattleOptions.hostType == HOST_SPADS)
			SendToServer("SAYPRIVATE %s !vote %c", gMyBattle->founder, voteYes ? 'y' : 'n');
		else if (gBattleOptions.hostType == HOST_SPRINGIE)
			SendToServer("SAYBATTLE !vote %c", '2' - (char)voteYes);
		EnableWindow(GetDlgItem(gBattleRoom, DLG_VOTEYES), 0);
		EnableWindow(GetDlgItem(gBattleRoom, DLG_VOTENO), 0);
		EnableWindow(GetDlgItem(gBattleRoom, DLG_VOTETEXT), 0);
		return 0;
	}
	return 1;
}

void BattleRoom_OnResync(void)
{
	const char *def; int defIndex;

	SendDlgItemMessage(gBattleRoom, DLG_MOD, CB_RESETCONTENT, 0, 0);
	def = LoadSetting("last_host_mod"); defIndex=0;
	for (int i=0; i<gNbMods; ++i) {
		if (def && !strcmp(def, gMods[i]))
			defIndex = i;
		SendDlgItemMessageA(gBattleRoom, DLG_MOD, CB_ADDSTRING, 0, (LPARAM)gMods[i]);
	}
	SendDlgItemMessage(gBattleRoom, DLG_MOD, CB_SETCURSEL, defIndex, 0);


	SendDlgItemMessage(gBattleRoom, DLG_MAP, CB_RESETCONTENT, 0, 0);
	def = LoadSetting("last_host_map"); defIndex=0;
	for (int i=0; i<gNbMaps; ++i) {
		if (def && !strcmp(def, gMaps[i]))
			defIndex = i;
		SendDlgItemMessageA(gBattleRoom, DLG_MAP, CB_ADDSTRING, 0, (LPARAM)gMaps[i]);
	}
	SendDlgItemMessage(gBattleRoom, DLG_MAP, CB_SETCURSEL, defIndex, 0);


}

void BattleRoom_OnSetModDetails(void)
{
	HWND infoList = GetDlgItem(gBattleRoom, DLG_BATTLE_INFO);

	SendMessage(infoList, LVM_REMOVEALLGROUPS, 0, 0);
	ListView_DeleteAllItems(infoList);

	for (ssize_t i=0; i<gNbModOptions; ++i) {
		if (gModOptions[i].type != opt_section)
			continue;
		LVGROUP group = {
			sizeof(LVGROUP),
			LVGF_HEADER | LVGF_GROUPID,
			utf8to16(gModOptions[i].name),
			.iGroupId = (intptr_t)&gModOptions[i]
		};
		SendMessage(infoList, LVM_INSERTGROUP, -1, (LPARAM)&group);
	}

	for (ssize_t i=0; i<gNbModOptions; ++i) {
		if (gModOptions[i].type == opt_section)
			continue;

		LVITEMA item = {
			LVIF_GROUPID | LVIF_TEXT, INT_MAX,
			.pszText = gModOptions[i].name,
			.iGroupId = (intptr_t)gModOptions[i].section
		};

		int id = SendMessageA(infoList, LVM_INSERTITEMA, 0, (LPARAM)&item);

		item.mask = LVIF_TEXT;
		item.iItem = id;
		item.iSubItem = 1;
		item.pszText = gModOptions[i].val;
		SendMessageA(infoList, LVM_SETITEMA, 0, (LPARAM)&item);
	}

	ListView_SetColumnWidth(infoList, 0, LVSCW_AUTOSIZE);
	ListView_SetColumnWidth(infoList, 1, LVSCW_AUTOSIZE);
}

void BattleRoom_OnChangeMod(void)
{
	// HWND sideBox = GetDlgItem(gBattleRoom, DLG_SIDE);
	// SendDlgItemMessage(sideBox, DLG_SIDE, CB_RESETCONTENT, 0, 0);

	for (int i=0; i<=DLG_SIDE_LAST - DLG_SIDE_FIRST; ++i) {
		HWND sideButton = GetDlgItem(gBattleRoom, DLG_SIDE_FIRST + i);
		SendMessage(sideButton, BM_SETIMAGE, IMAGE_ICON, (WPARAM)ImageList_GetIcon(gIconList, ICONS_FIRST_SIDE + i, 0));
		ShowWindow(sideButton, i < gNbSides);
	}
	// SendDlgItemMessage(gBattleRoom, DLG_SPLIT_HORZ, BM_SETIMAGE, IMAGE_ICON, (WPARAM)ImageList_GetIcon(gIconList, ICONS_SPLIT_VERT, 0));
	// for (int i=0; *gSideNames[i]; ++i)
	// SendMessage(sideBox, CBEM_INSERTITEMA, 0,
	// (LPARAM)&(COMBOBOXEXITEMA){
	// .iItem = -1, .mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE,
	// .pszText = gSideNames[i],
	// .iImage = ICONS_FIRST_SIDE + i, .iSelectedImage = ICONS_FIRST_SIDE + i,
	// });
	HWND playerList = GetDlgItem(gBattleRoom, DLG_PLAYER_LIST);
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
	// SendMessage(sideBox, CB_SETCURSEL, &gMyUser ? FROM_SIDE_MASK(gMyUser.battleStatus) : 0, 0);
	// resizePlayerListTabs();
	if (gBattleOptions.startPosType == STARTPOS_CHOOSE_INGAME)
		for (int i=0; i<16; ++i)
			SetWindowPos(GetDlgItem(GetDlgItem(gBattleRoom, DLG_MINIMAP), DLG_FIRST_STARTPOS + i), NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW);
}

static LRESULT CALLBACK battleRoomProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CREATE:
		onCreate(window);
		return 0;
	case WM_SIZE:
		resizeAll(lParam);
		// if (!wParam)
		break;
		//FALLTHROUGH:
		// case WM_EXITSIZEMOVE:
		// InvalidateRect(GetDlgItem(gBattleRoom, DLG_MINIMAP), 0, 0);
		// break;
	case WM_DRAWITEM:
		assert(wParam == DLG_MINIMAP);
		onDraw((void *)lParam);
		return 1;
	case WM_NOTIFY:
		return onNotify(wParam, (NMHDR *)lParam);
	case WM_DESTROY:
		gBattleRoom = NULL;
		break;
	case WM_HSCROLL:
		if (GetDlgCtrlID((HWND)lParam) != DLG_SPLIT_SIZE
				|| wParam != SB_ENDSCROLL)
			return 0;

		for (int i=0; i<=SPLIT_LAST; ++i)
			if (SendDlgItemMessage(gBattleRoom, DLG_SPLIT_FIRST + i, BM_GETCHECK, 0, 0))
				SetSplit(i, SendDlgItemMessage(window, DLG_SPLIT_SIZE, TBM_GETPOS, 0, 0));
		// SendDlgItemMessage(window, DLG_SPLIT_SIZE, TBM_GETPOS, 0, 0)
		return 0;
	case WM_CLOSE:
		MainWindow_DisableBattleroomButton();
		LeaveBattle();
		return 0;
	case WM_COMMAND:
		return onCommand(wParam, (HWND)lParam);
	}
	return DefWindowProc(window, msg, wParam, lParam);
}

void BattleRoom_VoteStarted(const char *topic)
{
	SetDlgItemTextA(gBattleRoom, DLG_VOTETEXT, topic);
	EnableWindow(GetDlgItem(gBattleRoom, DLG_VOTETEXT), (BOOL)topic);
	EnableWindow(GetDlgItem(gBattleRoom, DLG_VOTEYES), (BOOL)topic);
	EnableWindow(GetDlgItem(gBattleRoom, DLG_VOTENO), (BOOL)topic);
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
	GetDlgItemTextA(gBattleRoom, DLG_MOD, gMyBattle->modName, sizeof(gMyBattle->modName));
	GetDlgItemTextA(gBattleRoom, DLG_MAP, gMyBattle->mapName, sizeof(gMyBattle->mapName));

	ChangedMod(gMyBattle->modName);
	if (!gMapHash || gMapHash != GetMapHash(gMyBattle->mapName) || !gMapHash)
		ChangedMap(gMyBattle->mapName);
	gMyBattle->mapHash = gMapHash;
	gBattleOptions.modHash = gModHash;

	gMyUser.battleStatus = READY_MASK | MODE_MASK | SYNC_MASK;
	gMyUser.battle = gMyBattle;
	SendDlgItemMessage(gBattleRoom, DLG_SPECTATE, BM_SETCHECK, 0, 0);
	gMyBattle->founder = &gMyUser;
	gMyBattle->nbParticipants = 1;

	BattleRoom_Show();
}
