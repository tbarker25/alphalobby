#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include <windows.h>
#include <Commctrl.h>
#include "wincommon.h"

#include "battlelist.h"
#include "chat.h"
#include "chat_window.h"
#include "client_message.h"
#include "data.h"
#include "downloader.h"
#include "imagelist.h"
#include "listview.h"
#include "sync.h"

HWND gBattleList;

#define LENGTH(x) (sizeof(x) / sizeof(*x))


enum DLG_ID {
	DLG_LIST,
	DLG_MAP,
	DLG_BATTLE_INFO,
	DLG_BATTLE_LIST,
	DLG_JOIN,
	DLG_LAST = DLG_JOIN,
};

static const DialogItem dialogItems[] = {
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

static void SortBattleList(int newOrder)
{
	static int sortOrder = 5, reverseSort = 0;
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
				[3] = offsetof(Battle, modName),
				[4] = offsetof(Battle, mapName),
			};
			s1 = (void *)b1 + offsets[sortOrder];
			s2 = (void *)b2 + offsets[sortOrder];
		}
		return stricmp(s1, s2);
	}

	ListView_SortItems(GetDlgItem(gBattleList, DLG_LIST), CompareFunc, 0);
}

static void resizeColumns(void)
{
	RECT rect;
	HWND list = GetDlgItem(gBattleList, DLG_LIST);
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
	SendDlgItemMessage(gBattleList, DLG_LIST, LVM_GETITEM, 0, (LPARAM)&item);
	return (Battle *)item.lParam;
}

static void onItemRightClick(POINT pt)
{
	int index = SendDlgItemMessage(gBattleList, DLG_LIST,
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
	if (!GetMapHash(b->mapName))
		AppendMenu(menu, 0, DL_MAP, L"Download map");
	if (!GetModHash(b->modName))
		AppendMenu(menu, 0, DL_MOD, L"Download mod");


	FOR_EACH_USER(u, b)
		AppendMenuA(userMenu, 0, (UINT_PTR)u, u->name);

	InsertMenu(userMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	ClientToScreen(gBattleList, &pt);

	int clicked = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y,
			gBattleList, NULL);
	switch (clicked) {
	case 0:
		break;
	case 1:
		JoinBattle(b->id, NULL);
		break;
	case DL_MAP:
		DownloadMap(b->mapName); 
		break;
	case DL_MOD:
		DownloadMod(b->modName);
		break;
	default:
		ChatWindow_SetActiveTab(GetPrivateChat((User *)clicked));
		break;
	}

	DestroyMenu(userMenu);
	DestroyMenu(menu);
}

static void onGetInfoTip(NMLVGETINFOTIP *info)
{
	Battle *b = getBattleFromIndex(info->iItem);
	swprintf(info->pszText,
			L"%hs\n%hs\n%hs\n%s\n%d/%d players - %d spectators",
			b->founder->name, b->modName, b->mapName,
			utf8to16(b->title), GetNumPlayers(b), b->maxPlayers,
			b->nbParticipants);
}

static void onCreate(HWND window)
{
	gBattleList = window;
	CreateDlgItems(window, dialogItems, DLG_LAST + 1);

	HWND listDlg = GetDlgItem(gBattleList, DLG_LIST);

	LVCOLUMN columnInfo = { LVCF_TEXT | LVCF_SUBITEM };
	for (int i=0, n=sizeof(columns) / sizeof(char *); i < n; ++i) {
		columnInfo.pszText = (wchar_t *)columns[i];
		columnInfo.iSubItem = i;
		ListView_InsertColumn(listDlg, i, &columnInfo);
	}

	EnableIcons(listDlg);
	ListView_SetExtendedListViewStyle(listDlg,
			LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP
			| LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
}

static LRESULT CALLBACK battleListProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CLOSE:
		return 0;
	case WM_CREATE:
		onCreate(window);
		return 0;
	case WM_SIZE:
		MoveWindow(GetDlgItem(window, DLG_LIST), 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		resizeColumns();
		return 0;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {

		case LVN_COLUMNCLICK:
			SortBattleList(((NMLISTVIEW *)lParam)->iSubItem + 1);
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


void BattleList_CloseBattle(Battle *b)
{
	LVFINDINFO findInfo = {.flags = LVFI_PARAM, .lParam = (LPARAM)b};
	HWND list = GetDlgItem(gBattleList, DLG_LIST);
	int index = ListView_FindItem(list, -1, &findInfo);
	ListView_DeleteItem(list, index);
}

void BattleList_UpdateBattle(Battle *b)
{
	HWND list = GetDlgItem(gBattleList, DLG_LIST);
	LVFINDINFO findInfo = {LVFI_PARAM, .lParam = (LPARAM)b};
	LVITEM item;
	item.iItem = ListView_FindItem(list, -1, &findInfo);

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
	if (b->founder->clientStatus & CS_INGAME_MASK)
		iconIndex |= INGAME_MASK;
	if (b->passworded)
		iconIndex |= PW_MASK;
	if (!b->locked && b->maxPlayers == GetNumPlayers(b))
		iconIndex |= FULL_MASK;
	item.state = INDEXTOOVERLAYMASK(iconIndex);

	ListView_SetItem(list, &item);

	item.mask = LVIF_TEXT;

#define ADD_STRING(s) \
	++item.iSubItem; \
	item.pszText = s; \
	SendMessage(list, LVM_SETITEM, 0, (LPARAM)&item); \

	ADD_STRING(utf8to16(b->title));
	ADD_STRING(utf8to16(b->modName));
	ADD_STRING(utf8to16(b->mapName));
	wchar_t buff[16];
	swprintf(buff, L"%d / %d +%d",
			GetNumPlayers(b), b->maxPlayers, b->nbSpectators);
	ADD_STRING(buff);

#undef ADD_STRING

	SortBattleList(0);
}

void BattleList_OnEndLoginInfo(void)
{
	SortBattleList(0);
	resizeColumns();
}

	__attribute__((constructor))
static void _init_ (void)
{
	WNDCLASSEX classInfo = {
		.lpszClassName = WC_BATTLELIST,
		.cbSize        = sizeof(WNDCLASSEX),
		.lpfnWndProc   = battleListProc,
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	};
	RegisterClassEx(&classInfo);
}

void BattleList_Reset(void)
{
	SendDlgItemMessage(gBattleList, DLG_LIST, LVM_DELETEALLITEMS, 0, 0);
}
