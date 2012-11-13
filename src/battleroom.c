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
	
	DLG_MAPMODE_LABEL,
	DLG_MAPMODE_MINIMAP,
	DLG_MAPMODE_METAL,
	DLG_MAPMODE_ELEVATION,
	DLG_CHANGE_MAP,
	
	DLG_VOTEBOX,
	DLG_VOTETEXT,
	DLG_VOTEYES,
	DLG_VOTENO,
	
	DLG_LAST = DLG_VOTENO,
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
	if (!battleInfoFinished)
		return;
	static int nbCalls;
	printf("start positions changed %d\n", ++nbCalls);
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

	BattleRoom_RedrawMinimap();
}

void BattleRoom_Show(void)
{
	RECT rect;
	GetClientRect(gBattleRoom, &rect);
	SendMessage(gBattleRoom, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
	MainWindow_EnableBattleroomButton();
}

void BattleRoom_Hide(void)
{
	ListView_DeleteAllItems(GetDlgItem(gBattleRoom, DLG_PLAYERLIST));
	MainWindow_DisableBattleroomButton();
}

static int findUser(const void *u)
{
	LVFINDINFO findInfo;
	findInfo.flags = LVFI_PARAM;
	findInfo.lParam = (LPARAM)u;
	return SendDlgItemMessage(gBattleRoom, DLG_PLAYERLIST, LVM_FINDITEM,
			-1, (LPARAM)&findInfo);
}

void BattleRoom_RemoveUser(const union UserOrBot *s)
{
	SendDlgItemMessage(gBattleRoom, DLG_PLAYERLIST, LVM_DELETEITEM, findUser(s), 0);
}

static const char * getEffectiveUsername(const union UserOrBot *s)
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
	
	LVGROUP groupInfo;
	groupInfo.cbSize = sizeof(groupInfo);
	groupInfo.mask = LVGF_HEADER;
	groupInfo.pszHeader = buff;
	SendDlgItemMessage(gBattleRoom, DLG_PLAYERLIST, LVM_SETGROUPINFO,
			groupId, (LPARAM)&groupInfo);
}

char BattleRoom_IsAutoUnspec(void)
{
	return SendDlgItemMessage(gBattleRoom, DLG_AUTO_UNSPEC, BM_GETCHECK, 0, 0);
}

void BattleRoom_UpdateUser(union UserOrBot *s)
{
	HWND playerList = GetDlgItem(gBattleRoom, DLG_PLAYERLIST);

	uint32_t battleStatus = s->battleStatus;
	int groupId = (battleStatus & MODE_MASK) ? FROM_ALLY_MASK(battleStatus) : 16;

	LVITEM item;
	item.iItem = findUser(s);
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
		updatePlayerListGroup(item.iGroupId);
		item.iGroupId = groupId;
		ListView_SetItem(playerList, &item);
	}

	updatePlayerListGroup(groupId);

	item.mask = LVIF_STATE | LVIF_IMAGE;
	item.iSubItem = COLUMN_STATUS;
	item.stateMask = LVIS_OVERLAYMASK;

	if (!(battleStatus & AI_MASK)) {
		item.iImage = &s->user == gMyBattle->founder ? (battleStatus & MODE_MASK ? ICONS_HOST : ICONS_HOST_SPECTATOR)
			: s->user.clientStatus & CS_INGAME_MASK ? ICONS_INGAME
			: !(battleStatus & MODE_MASK) ? ICONS_SPECTATOR
			: battleStatus & READY_MASK ? ICONS_READY
			: ICONS_UNREADY;
		int iconIndex = USER_MASK;
		if (!(battleStatus & SYNCED))
			iconIndex |= UNSYNC_MASK;
		if (s->user.clientStatus & CS_AWAY_MASK)
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
	if (battleStatus & MODE_MASK && *gSideNames[FROM_SIDE_MASK(battleStatus)])
		sideIcon = ICONS_FIRST_SIDE + FROM_SIDE_MASK(battleStatus);
	item.mask = LVIF_IMAGE;
	setIcon(COLUMN_SIDE, sideIcon);


	extern int GetColorIndex(const union UserOrBot *);
	setIcon(COLUMN_COLOR, GetColorIndex((void *)s));

	if (battleStatus & AI_MASK) {
		wchar_t name[MAX_NAME_LENGTH * 2 + 4];
		swprintf(name, L"%hs (%hs)", s->name, s->bot.dll);
		item.mask = LVIF_TEXT;
		item.iSubItem = COLUMN_NAME;
		item.pszText = name;
		ListView_SetItem(playerList, &item);
		goto sort;
	}

	User *u = &s->user;


	assert(item.mask = LVIF_IMAGE);
	setIcon(COLUMN_FLAG, ICONS_FIRST_FLAG + u->country);
	setIcon(COLUMN_RANK, ICONS_FIRST_RANK + FROM_RANK_MASK(u->clientStatus));

	if (u == &gMyUser) {
		SendDlgItemMessage(gBattleRoom, DLG_SPECTATE, BM_SETCHECK,
				!(battleStatus & MODE_MASK), 0);

		EnableWindow(GetDlgItem(gBattleRoom, DLG_AUTO_UNSPEC),
				!(battleStatus & MODE_MASK));

		if (battleStatus & MODE_MASK)
			SendDlgItemMessage(gBattleRoom, DLG_AUTO_UNSPEC,
					BM_SETCHECK, BST_UNCHECKED, 0);

		for (int i=0; i<=NUM_SIDE_BUTTONS; ++i)
			SendDlgItemMessage(gBattleRoom, DLG_SIDE_FIRST + i, BM_SETCHECK,
					FROM_SIDE_MASK(battleStatus) == i, 0);

		SendDlgItemMessage(gBattleRoom, DLG_ALLY, CB_SETCURSEL,
				FROM_ALLY_MASK(battleStatus), 0);
	}

	if (u->battle->founder == u) {
		HWND startButton = GetDlgItem(gBattleRoom, DLG_START);
		int canJoin = !(gMyUser.clientStatus & CS_INGAME_MASK)
			&& (u->clientStatus & CS_INGAME_MASK
					|| gBattleOptions.hostType & HOST_FLAG);
		EnableWindow(startButton, canJoin);
	}

sort:;
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

		item.mask = LVIF_TEXT;
		item.iItem = findUser(u);
		item.iSubItem = COLUMN_NAME;
		item.pszText = buff;

		ListView_SetItem(playerList, &item);
	}
	SendMessage(playerList, LVM_SORTITEMS, 0, (LPARAM)sortPlayerList);
}

void resizePlayerListTabs(void)
{
	HWND playerList = GetDlgItem(gBattleRoom, DLG_PLAYERLIST);
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


	MOVE_ID(DLG_INFOLIST, XS, YS, INFO_WIDTH, INFO_HEIGHT);
	MOVE_ID(DLG_PLAYERLIST, INFO_WIDTH + 2*XS, YS, LIST_WIDTH, INFO_HEIGHT);
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
	BattleRoom_RedrawMinimap();
}

static RECT boundingRect;
static LRESULT CALLBACK tooltipSubclass(HWND window, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (msg == WM_MOUSEMOVE
			&& (GET_X_LPARAM(lParam) < boundingRect.left
				|| GET_X_LPARAM(lParam) > boundingRect.right
				|| GET_Y_LPARAM(lParam) < boundingRect.top
				|| GET_Y_LPARAM(lParam) > boundingRect.bottom))
		SendMessage((HWND)dwRefData, TTM_POP, 0, 0);
	return DefSubclassProc(window, msg, wParam, lParam);
}

static void onCreate(HWND window)
{
	gBattleRoom = window;
	CreateDlgItems(window, dialogItems, DLG_LAST + 1);
	for (int i=0; i<16; ++i) {
		wchar_t buff[LENGTH("Team 16")];
		swprintf(buff, L"Team %d", i+1);
		SendDlgItemMessage(window, DLG_ALLY, CB_ADDSTRING, 0, (LPARAM)buff);
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
	SendMessage(playerList, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)gIconList);
	ListView_EnableGroupView(playerList, TRUE);
	for (int i=0; i<=16; ++i) {
		wchar_t buff[LENGTH("Spectators")];
		swprintf(buff, i<16 ? L"Team %d" : L"Spectators", i+1);
		LVGROUP groupInfo;
		groupInfo.cbSize = sizeof(groupInfo);
		groupInfo.mask = LVGF_HEADER | LVGF_GROUPID;
		groupInfo.pszHeader = buff;
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
	SetWindowSubclass(playerList, tooltipSubclass, 0, (DWORD_PTR)hwndTip);

	toolInfo.uId = (UINT_PTR)infoList;
	SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	SetWindowSubclass(infoList, tooltipSubclass, 0, (DWORD_PTR)hwndTip);

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
	static int nbRedraw;
	printf("REDRAWING %d\n", ++nbRedraw);
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
	if (!gMyBattle)
		goto cleanup;

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
				pixels[x+width*(yMax-1)] = 0;
			}
			for (int y=yMin; y<yMax; ++y) {
				pixels[xMin+width*y] = 0;
				pixels[(xMax-1)+width*y] = 0;
			}
		}

	} else {
		int max = gMyBattle ? GetNumPlayers(gMyBattle) : 0;
		max = max < gMapInfo.posCount ? max : gMapInfo.posCount;
		for (int i=0; i<max; ++i) {
			int xMid = gMapInfo.positions[i].x * width / gMapInfo.width;
			int yMid = gMapInfo.positions[i].z * height / gMapInfo.height;

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

static void getUserTooltip(User *u, wchar_t *buff)
{
	wchar_t *s = buff;

	s += swprintf(s, L"%hs", u->name);
	if (strcmp(GetAliasOf(u->name), u->alias))
		s += swprintf(s, L" (%hs)", u->alias);

	if (!(u->battleStatus & AI_MASK))
		s += swprintf(s, L"\nRank %d - %hs - %.2fGHz\n",
				FROM_RANK_MASK(u->clientStatus),
				countryNames[u->country],
				(float)u->cpu / 1000);

	if (!(u->battleStatus & MODE_MASK)) {
		s += swprintf(s, L"Spectator");
		return;
	}

	const char *sideName = gSideNames[FROM_SIDE_MASK(u->battleStatus)];

	s += swprintf(s, L"Player %d - Team %d",
			FROM_TEAM_MASK(u->battleStatus),
			FROM_ALLY_MASK(u->battleStatus));
	if (*sideName)
		s += swprintf(s, L" - %hs", sideName);

	if (u->battleStatus & HANDICAP_MASK)
		s += swprintf(s, L"\nHandicap: %d",
				FROM_HANDICAP_MASK(u->battleStatus));

	assert(s - buff < INFOTIPSIZE);
}

static LRESULT onNotify(WPARAM wParam, NMHDR *note)
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

			getUserTooltip((User *)item.lParam, ((NMTTDISPINFO *)note)->lpszText);
			return 0;
		}
		((NMTTDISPINFO *)note)->lpszText = utf8to16(((Option *)item.lParam)->desc);
		return 0;

	case LVN_ITEMACTIVATE:
		item.iItem = ((LPNMITEMACTIVATE)note)->iItem;
		SendMessage(note->hwndFrom, LVM_GETITEM, 0, (LPARAM)&item);
		if (note->idFrom == DLG_INFOLIST)
			ChangeOption((Option *)item.lParam);
		else if (note->idFrom == DLG_PLAYERLIST)
			ChatWindow_SetActiveTab(GetPrivateChat((User *)item.lParam));
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
		CreateUserMenu((UserOrBot *)item.lParam, gBattleRoom);
		return 1;

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

		//TODO: This seems wrong to me. Too lazy to fix.
	case MAKEWPARAM(DLG_SPLIT_FIRST, BN_CLICKED) ... MAKEWPARAM(DLG_SPLIT_LAST, BN_CLICKED):
		SetSplit(LOWORD(wParam) - DLG_SPLIT_FIRST, SendDlgItemMessage(gBattleRoom, DLG_SPLIT_SIZE, TBM_GETPOS, 0, 0));
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

static void setDetails(const Option *options, ssize_t nbOptions)
{
	HWND infoList = GetDlgItem(gBattleRoom, DLG_INFOLIST);

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

/* static void setMapInfo(void) */
/* { */
/* HWND infoList = GetDlgItem(gBattleRoom, DLG_INFOLIST); */
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

/* if (gMapInfo.author[0]) */
/* ADD_STR(L"Author", utf8to16(gMapInfo.author)); */
/* ADD_STR(L"Tidal: ", utf8to16(gMapInfo.author)); */

/* #undef ADD_STR */
/* } */

void BattleRoom_OnSetModDetails(void)
{
	HWND infoList = GetDlgItem(gBattleRoom, DLG_INFOLIST);
	SendMessage(infoList, LVM_REMOVEALLGROUPS, 0, 0);
	ListView_DeleteAllItems(infoList);

	setDetails(gModOptions, gNbModOptions);
	setDetails(gMapOptions, gNbMapOptions);
	/* setMapInfo(); */

	ListView_SetColumnWidth(infoList, 0, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(infoList, 1, LVSCW_AUTOSIZE_USEHEADER);
}

void BattleRoom_OnSetOption(Option *opt)
{
	HWND infoList = GetDlgItem(gBattleRoom, DLG_INFOLIST);

	LVFINDINFO findInfo;
	findInfo.flags = LVFI_PARAM;
	findInfo.lParam = (LPARAM)opt;

	LVITEMA item;
	item.mask = LVIF_TEXT;
	item.iSubItem = 1;
	item.pszText = opt->val ?: opt->def;
	item.iItem = ListView_FindItem(infoList, -1, &findInfo);

	SendMessageA(infoList, LVM_SETITEMA, 0, (LPARAM)&item);
}

void BattleRoom_OnChangeMod(void)
{
	for (int i=0; i<=DLG_SIDE_LAST - DLG_SIDE_FIRST; ++i) {
		HWND sideButton = GetDlgItem(gBattleRoom, DLG_SIDE_FIRST + i);
		SendMessage(sideButton, BM_SETIMAGE, IMAGE_ICON, (WPARAM)ImageList_GetIcon(gIconList, ICONS_FIRST_SIDE + i, 0));
		ShowWindow(sideButton, i < gNbSides);
	}
	HWND playerList = GetDlgItem(gBattleRoom, DLG_PLAYERLIST);
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
}

static LRESULT CALLBACK battleRoomProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CREATE:
		onCreate(window);
		return 0;
	case WM_SIZE:
		resizeAll(lParam);
		break;
	case WM_DRAWITEM:
		if (wParam == DLG_MINIMAP)
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

