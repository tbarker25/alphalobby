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
	LV_ITEM item = {
		.mask = LVIF_PARAM,
		.iItem = index
	};
	ListView_GetItem(GetDlgItem(gBattleList, DLG_LIST), &item);
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

	int clicked = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y, gBattleList, NULL);
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

static LRESULT CALLBACK battleListProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
	case WM_CLOSE:
		return 0;
	case WM_CREATE:
		gBattleList = window;
		CreateDlgItems(window, dialogItems, DLG_LAST + 1);
		
		HWND listDlg = GetDlgItem(gBattleList, DLG_LIST);
		
		for (int i=0, n=sizeof(columns) / sizeof(char *); i < n; ++i) {
			ListView_InsertColumn(listDlg, i, (&(LVCOLUMN){
				.mask = LVCF_TEXT | LVCF_SUBITEM,
				.pszText = (wchar_t *)columns[i],
				.iSubItem = i,
			}));
		}
		EnableIcons(listDlg);
		ListView_SetExtendedListViewStyle(listDlg, LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
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
	ListView_DeleteItem(GetDlgItem(gBattleList, DLG_LIST), ListView_FindItem(GetDlgItem(gBattleList, DLG_LIST), -1, (&(LVFINDINFO){
		.flags = LVFI_PARAM,
		.lParam = (LPARAM)b,
	})));
}

void BattleList_UpdateBattle(Battle *b)
{
	HWND list = GetDlgItem(gBattleList, DLG_LIST);
	int item = ListView_FindItem(list, -1,
		(&(LVFINDINFO){.flags = LVFI_PARAM, .lParam = (LPARAM)b})
	);

	if (item == -1)
		item = SendMessage(list, LVM_INSERTITEMA, 0,
				(LPARAM)&(LVITEMA){.mask = LVIF_PARAM | LVIF_TEXT, .lParam = (LPARAM)b, .pszText = b->founder->name});

	ListView_SetItem(list, (&(LVITEM){
		.iItem = item,
		.mask = LVIF_STATE | LVIF_IMAGE,
		.iImage = b->locked ? ICONS_CLOSED : ICONS_OPEN,
		.stateMask = LVIS_OVERLAYMASK,
		.state = INDEXTOOVERLAYMASK(((b->founder->clientStatus & CS_INGAME_MASK) ? INGAME_MASK : 0)
		       | b->passworded * PW_MASK
			   | (b->maxPlayers == GetNumPlayers(b)) * FULL_MASK * !b->locked)
		})
	);

	int column = 0;
	void AddText(char text[])
	{
		SendMessage(list, LVM_SETITEM, 0, (LPARAM)&(LVITEM){
			.mask = LVIF_TEXT,
			.iItem = item,
			.iSubItem = ++column,
			.pszText = utf8to16(text)}
		);
	}
	void AddInt(int64_t n)
	{
		char buff[128];
		snprintf(buff, 128, "%" PRId64, n);
		AddText(buff);
	}

	AddText(b->title);
	AddText(b->modName);
	AddText(b->mapName);
	char buff[128];
	sprintf(buff, "%d / %d +%d", GetNumPlayers(b), b->maxPlayers, b->nbSpectators);
	AddText(buff);

	SortBattleList(0);
}

void BattleList_OnEndLoginInfo(void)
{
	SortBattleList(0);
	resizeColumns();
}

static void
__attribute__((constructor))
_init_ (void)
{
	RegisterClassEx((&(WNDCLASSEX){
		.lpszClassName = WC_BATTLELIST,
		.cbSize        = sizeof(WNDCLASSEX),
		.lpfnWndProc   = battleListProc,
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	}));
}

void BattleList_Reset(void)
{
	SendDlgItemMessage(gBattleList, DLG_LIST, LVM_DELETEALLITEMS, 0, 0);
}
