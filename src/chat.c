#include <stdio.h>
#include <sys\stat.h>
#include <ctype.h>
#include <assert.h>
#include <malloc.h>

#include "common.h"
#include "wincommon.h"
#include "layoutmetrics.h"

#include <windowsx.h>
#include <Commctrl.h>

#include "client_message.h"
#include "data.h"

#include "alphalobby.h"
#include "settings.h"
#include "chat.h"
#include "chat_window.h"
#include "resource.h"
#include "sync.h"
#include "userlist.h"
#include "imagelist.h"
#include "battletools.h"
#include "battleroom.h"
#include "richedit.h"
#include "dialogboxes.h"
#include "spring.h"



// #define SERVER_ID 0x2000
// #define CHANNEL_ID 0x2001
// #define PRIVATE_CHAT_ID 0x2002

static HWND channelWindows[128];
HWND _gServerChatWindow;

#define MARGIN (MAP_X(115))

enum {
	COLUMN_NAME,
	COLUMN_COUNTRY,
	COLUMN_LAST = COLUMN_COUNTRY,
};

enum {
	DLG_LOG,
	DLG_INPUT,
	DLG_LIST,
	DLG_LAST = DLG_LIST,
};

typedef struct chatWindowData {
	char *name;
	ChatDest type;
}chatWindowData;

static DialogItem dialogItems[] = {
	[DLG_LOG] = {
		.class = RICHEDIT_CLASS,
		.exStyle = WS_EX_WINDOWEDGE,
		.style = WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
	}, [DLG_INPUT] = {
		.class = WC_EDIT,
		.exStyle = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | ES_AUTOHSCROLL,
	}, [DLG_LIST] = {
		.class = WC_LISTVIEW,
		.exStyle = WS_EX_CLIENTEDGE,
		.style = WS_VISIBLE | LVS_SHAREIMAGELISTS | LVS_SINGLESEL | LVS_REPORT | LVS_SORTASCENDING | LVS_NOCOLUMNHEADER,
	}
};

void ChatWindow_RemoveUser(HWND window, User *u)
{
	window = GetDlgItem(window, DLG_LIST);
	int i = SendMessage(window, LVM_FINDITEM, -1,
			(LPARAM)&(LVFINDINFO){.flags = LVFI_PARAM, .lParam = (LPARAM)u});
	SendMessage(window, LVM_DELETEITEM, i, 0);
}

static void updateUser(HWND window, User *u, int item)
{
	char *name;
	if (!strcmp(u->name, u->alias))
		name = u->name;
	else {
		name = alloca(MAX_NAME_LENGTH * 2 + sizeof(" ()"));
		sprintf(name, "%s (%s)", u->name, u->alias);
	}

	SendMessage(window, LVM_SETITEMA, 0, (LPARAM)&(LVITEMA){
		.mask = LVIF_IMAGE | LVIF_STATE | LVIF_TEXT,
		.iItem = item,
		.pszText = name,
		.iImage = (u->clientStatus & CS_INGAME_MASK) ? ICONS_INGAME : u->battle ? INGAME_MASK : -1,
		.stateMask = LVIS_OVERLAYMASK,
		.state = INDEXTOOVERLAYMASK(USER_MASK | !!(u->clientStatus & CS_AWAY_MASK) * AWAY_MASK | u->ignore * IGNORE_MASK)
	});
	SendMessage(window, LVM_SETITEM, 0, (LPARAM)&(LVITEM){
		.mask = LVIF_IMAGE, .iImage = ICONS_FIRST_FLAG + u->country,
		.iItem = item, .iSubItem = COLUMN_COUNTRY,
	});
}

void UpdateUser(User *u)
{
	// int i=0x2000;
	// HWND window = GetServerChat();
	// do {
		// window = GetDlgItem(window, DLG_LIST);
		// int item = SendMessage(window, LVM_FINDITEM, -1,
			// (LPARAM)&(LVFINDINFO){.flags = LVFI_PARAM, .lParam = (LPARAM)u});
		// if (item >= 0)
			// updateUser(window, u, item);
	// } while ((window = GetDlgItem(gMainWindow, i++)));
	// if (gMyBattle && u->battle == gMyBattle)
		// BattleRoom_UpdateUser((void *)u);
}

void ChatWindow_AddUser(HWND window, User *u)
{
	window = GetDlgItem(window, DLG_LIST);
	int item = SendMessage(window, LVM_INSERTITEMA, 0, (LPARAM)&(LVITEMA){
				.mask = LVIF_PARAM | LVIF_TEXT, .lParam = (LPARAM)u, .pszText = u->name,
			});
	updateUser(window, u, item);
}

static const char *chatStrings[] = {"SAYBATTLE", "SAYPRIVATE ", "SAY "};
static const char *chatStringsEx[] = {"SAYBATTLEEX", "SAYPRIVATE ", "SAYEX "};


typedef struct inputBoxData_t {
	int lastIndex, end, lastPos, offset;
	wchar_t textBuff[8192], *inputHint, *buffTail;
}inputBoxData_t;

static LRESULT CALLBACK inputBoxProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR type, inputBoxData_t *data)
{
	if (msg != WM_KEYDOWN)
		goto done;
	
	if (wParam != VK_TAB)
		data->end = -1;

	switch (wParam) {
	case VK_TAB: {
		char text[MAX_TEXT_MESSAGE_LENGTH];
		GetWindowTextA(window, text, lengthof(text));
		
		if (data->lastPos != Edit_GetSel(window) || data->end < 0) {
			data->end = LOWORD(SendMessage(window, EM_GETSEL, 0, 0));
			data->offset = 0;
			data->lastIndex = 0;
		}
		int offset = data->offset, end = data->end + offset, start = data->end + offset;
		while (start - offset && !isspace(text[start - 1 - offset]))
			--start;

		text[end] = L'\0';
		
		HWND list = GetDlgItem(GetParent(type ? window : GetParent(window)), DLG_LIST);

		if (!list)
			return 0;
		
		int count = ListView_GetItemCount(list);
		if (!count)
			return 0;
		data->lastIndex %= count;
		LVITEM itemInfo = {LVIF_PARAM, data->lastIndex};
		do {
			SendMessage(list, LVM_GETITEM, 0, (LPARAM)&itemInfo);
			const char *name = ((User *)itemInfo.lParam)->name;
			const char *s = NULL;
			if (!_strnicmp(name, text+start, strlen(text+start)) || ((s = strchr(name, ']')) && !_strnicmp(s + 1, text+start, strlen(text+start)))) {
				data->lastIndex = itemInfo.iItem + 1;
				SendMessage(window, EM_SETSEL, start - offset, LOWORD(Edit_GetSel(window)));
				data->offset = s ? s - name + 1 : 0;
				SendMessageA(window, EM_REPLACESEL, 1, (LPARAM)name);
				break;
			}
			itemInfo.iItem = (itemInfo.iItem + 1) % count;
		} while (itemInfo.iItem != data->lastIndex);
		data->lastPos = Edit_GetSel(window);
	}	return 0;
	case VK_DOWN:
		if (!data->inputHint)
			return 0;
		while (++data->inputHint != data->textBuff
				&& data->inputHint != data->buffTail
				&& ((data->inputHint)[-1] || !*data->inputHint))
			if (data->inputHint >= data->textBuff+lengthof(data->textBuff)-1)
				data->inputHint = data->textBuff - 1;

		SetWindowText(window, data->inputHint);
		return 0;
	case VK_UP: {
		if (!data->inputHint)
			return 0;
		while ((--data->inputHint != data->textBuff)
				&& data->inputHint != data->buffTail
				&& ((data->inputHint)[-1] || !*data->inputHint))
			if (data->inputHint < data->textBuff)
				data->inputHint = data->textBuff + lengthof(data->textBuff) - 1;
		SetWindowText(window, data->inputHint);
	}	return 0;
	case '\r': {
		if (lengthof(data->textBuff) - (data->buffTail - data->textBuff) < MAX_TEXT_MESSAGE_LENGTH) {
			data->buffTail = data->textBuff;
			memset(data->buffTail, (lengthof(data->textBuff) - (data->buffTail - data->textBuff)) * sizeof(*data->textBuff), '\0');
		}
		size_t len = GetWindowTextW(window, data->buffTail, MAX_TEXT_MESSAGE_LENGTH);
		if (len <= 0)
			return 0;
		char textA[len * 3];
		WideCharToMultiByte(CP_UTF8, 0, data->buffTail, -1, textA, sizeof(textA), NULL, NULL);

		SetWindowText(window, NULL);
		wchar_t destName[128];
		GetWindowText(GetParent(window), destName, sizeof(destName));
		if (textA[0] == '/') {
			char *s = textA + 1;
			char *code = strsplit(&s, " ");
			
			if (!strcmp(code, "me")) {
				SendToServer("%s%s %s", chatStringsEx[type], utf16to8(destName), textA + lengthof("/me ") - 1);
			} else if (!strcmp(code, "start"))
				LaunchSpring();
			else if (!strcmp(code, "msg") || !strcmp(code, "pm")) {
				char *username = strsplit(&s, " ");
				User *u = FindUser(username);
				if (u) {
					SendToServer("SAYPRIVATE %s %s", u->name, s);
					ChatWindow_SetActiveTab(GetPrivateChat(u));
				} else {
					char buff[128];
					sprintf(buff, "Could not send message: %s is not logged in.", username);
					Chat_Said(GetParent(window), NULL, CHAT_SYSTEM, buff);
				}
			} else if (!strcmp(code, "j") || !strcmp(code, "join"))
				JoinChannel(s, 1);
			else if (!strcmp(code, "away"))
				SetClientStatus(~gMyUser.clientStatus, CS_AWAY_MASK);
			else if (!strcmp(code, "ingame"))
				SendToServer("GETINGAMETIME");
			else if (!strcmp(code, "split")) {
				char *splitType = strsplit(&s, " ");
				SplitType type = !strcmp(splitType, "h") ? SPLIT_HORZ
						 : !strcmp(splitType, "v") ? SPLIT_VERT
						 : !strcmp(splitType, "c1") ? SPLIT_CORNERS1
						 : !strcmp(splitType, "c2") ? SPLIT_CORNERS2
						 : SPLIT_LAST+1;
				if (type <= SPLIT_LAST)
					SetSplit(type, atoi(s));
			}
		} else if (type == DEST_SERVER)
			SendToServer("%s", textA);
		else 
			SendToServer("%s%s %s", chatStrings[type], utf16to8(destName), textA);

		SetWindowLongPtr(GetDlgItem(GetParent(window), DLG_LOG), GWLP_USERDATA, 0);
		data->buffTail += len+1;
		data->inputHint = data->buffTail;
	}	return 0;
	}
	done:
	return DefSubclassProc(window, msg, wParam, lParam);
}


static LRESULT CALLBACK logProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (msg == WM_VSCROLL)
		SetWindowLongPtr(window, GWLP_USERDATA, GetTickCount());

	return DefSubclassProc(window, msg, wParam, lParam);
}

static LRESULT CALLBACK chatBoxProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
	case WM_CREATE: {
		static chatWindowData battleRoomData = {NULL, DEST_BATTLE};
		chatWindowData *data = ((CREATESTRUCT *)lParam)->lpCreateParams ?: &battleRoomData;
		SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)data);
		if (data->name)
			SetWindowTextA(window, data->name);
		HWND logWindow = CreateDlgItem(window, &dialogItems[DLG_LOG], DLG_LOG);
		SendMessage(logWindow, EM_EXLIMITTEXT, 0, INT_MAX);
		SendMessage(logWindow, EM_AUTOURLDETECT, TRUE, 0);
		SendMessage(logWindow, EM_SETEVENTMASK, 0, ENM_LINK | ENM_MOUSEEVENTS | ENM_SCROLL);
		SetWindowSubclass(logWindow, logProc, 0, 0);

		HWND inputBox = CreateDlgItem(window, &dialogItems[DLG_INPUT], DLG_INPUT);
		inputBoxData_t *inputBoxData = calloc(1, sizeof(*inputBoxData));
		inputBoxData->buffTail = inputBoxData->textBuff;
		
		SetWindowSubclass(inputBox, (void *)inputBoxProc, (UINT_PTR)data->type, (DWORD_PTR)inputBoxData);
		
		if (data->type >= DEST_CHANNEL) {
			HWND list = CreateDlgItem(window, &dialogItems[DLG_LIST], DLG_LIST);
			for (int i=0; i<=COLUMN_LAST; ++i)
				SendMessage(list, LVM_INSERTCOLUMN, 0, (LPARAM)&(LVCOLUMN){});
			ListView_SetExtendedListViewStyle(list, LVS_EX_DOUBLEBUFFER | LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);
			EnableIcons(list);
		}
		// return 0;
	}	break;
	case WM_CLOSE: {
		chatWindowData *data = (void *)GetWindowLongPtr(window, GWLP_USERDATA);
		if (data->type == DEST_CHANNEL)
			LeaveChannel(data->name);
		RemoveTab(window);
	}	return 0;
	case WM_SIZE: {
		HWND list = GetDlgItem(window, DLG_LIST);
		HWND input = GetDlgItem(window, DLG_INPUT);
		MoveWindow(GetDlgItem(window, DLG_LOG),
			0, 0, LOWORD(lParam) - (list ? MAP_X(MARGIN) : 0), HIWORD(lParam) - !!input * MAP_Y(14), 1);
		MoveWindow(input,
			0, HIWORD(lParam) - MAP_Y(14), LOWORD(lParam) - (list ? MAP_X(MARGIN): 0), MAP_Y(14), 1);
		MoveWindow(list, LOWORD(lParam) - MAP_X(MARGIN), 0, MAP_X(MARGIN),HIWORD(lParam), 1);
		RECT rect;
		GetClientRect(list, &rect);
		SendMessage(list, LVM_SETCOLUMNWIDTH, 0, rect.right - 22);
		SendMessage(list, LVM_SETCOLUMNWIDTH, 1, 22);
	}	return 1;
	case WM_COMMAND:
		if (wParam == MAKEWPARAM(DLG_LOG, EN_VSCROLL))
			SetWindowLongPtr((HWND)lParam, GWLP_USERDATA, GetTickCount());
		return 0;
	case WM_NOTIFY: {
		NMHDR *note = (NMHDR *)lParam;
		if (note->idFrom == DLG_LIST) {
			LVITEM item = {
					.mask = LVIF_PARAM,
					.iItem = -1,
				};
			if (note->code == NM_RCLICK)
				item.iItem = SendMessage(note->hwndFrom, LVM_SUBITEMHITTEST, 0, (LPARAM)&(LVHITTESTINFO){.pt = ((LPNMITEMACTIVATE)lParam)->ptAction});
			else if (note->code == LVN_ITEMACTIVATE)
				item.iItem = ((LPNMITEMACTIVATE)lParam)->iItem;
			
			if (item.iItem < 0)
				break;
			
			SendMessage(note->hwndFrom, LVM_GETITEM, 0, (LPARAM)&item);
			User *u = (void *)item.lParam;
			if (u == &gMyUser)
				return 0;
			if (note->code == LVN_ITEMACTIVATE)
				goto addtab;
			HMENU menu = CreatePopupMenu();
			AppendMenu(menu, 0, 1, L"Private chat");
			AppendMenu(menu, u->ignore * MF_CHECKED, 2, L"Ignore");
			AppendMenu(menu, 0, 3, L"Set alias");
			SetMenuDefaultItem(menu, 1, 0);
			ClientToScreen(note->hwndFrom, &((LPNMITEMACTIVATE)lParam)->ptAction);
			int clicked = TrackPopupMenuEx(menu, TPM_RETURNCMD, ((LPNMITEMACTIVATE)lParam)->ptAction.x, ((LPNMITEMACTIVATE)lParam)->ptAction.y, window, NULL);
			DestroyMenu(menu);
			if (clicked == 1)
				addtab:
				ChatWindow_SetActiveTab(GetPrivateChat(u));
			else if (clicked == 2) {
				u->ignore ^= 1;
				UpdateUser(u);
			} else if (clicked == 3) {
				char title[128], buff[MAX_NAME_LENGTH_NUL];
				sprintf(title, "Set alias for %s", u->name);
				if (!GetTextDlg(title, strcpy(buff, u->name), MAX_NAME_LENGTH_NUL)) {
					strcpy(u->alias, buff);
				}
			}
			return 0;
			
		} else if (note->idFrom == DLG_LOG && note->code == EN_MSGFILTER) {
			MSGFILTER *s = (void *)lParam;
			if (s->msg != WM_RBUTTONUP)
				break;

			chatWindowData *data = (void *)GetWindowLongPtr(window, GWLP_USERDATA);
			
			HMENU menu = CreatePopupMenu();
			AppendMenu(menu, 0, 1, L"Copy");
			AppendMenu(menu, 0, 2, L"Clear window");
			AppendMenu(menu, gSettings.flags & SETTING_TIMESTAMP ? MF_CHECKED : 0, 3, L"Timestamp messages");

			if (data->type == DEST_PRIVATE)
				AppendMenu(menu, 0, 6, L"Ignore user");

			AppendMenu(menu, gSettings.flags & (1<<data->type) ? MF_CHECKED : 0, 4, L"Show login/logout");

			AppendMenu(menu, 0, 5, (const wchar_t *[]){L"Leave Battle", L"Close private chat", L"Leave channel", L"Hide server log"}[data->type]);
			
			POINT pt = {.x = LOWORD(s->lParam), .y = HIWORD(s->lParam)};
			ClientToScreen(s->nmhdr.hwndFrom, &pt);
			
			int index = TrackPopupMenuEx(menu, TPM_RETURNCMD, pt.x, pt.y, window, NULL);
			switch (index) {
			case 1: {
				CHARRANGE s;
				SendMessage(note->hwndFrom, EM_EXGETSEL, 0, (LPARAM)&s);
				HGLOBAL *memBuff = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * (s.cpMax - s.cpMin + 1));
				SendMessage(note->hwndFrom, EM_GETSELTEXT, 0, (LPARAM)GlobalLock(memBuff));
				GlobalUnlock(memBuff);
				OpenClipboard(window);
				EmptyClipboard();
				SetClipboardData(CF_UNICODETEXT, memBuff);
				CloseClipboard();
				break;
			}
			case 2:
				SetWindowText(s->nmhdr.hwndFrom, NULL);
				break;
			case 3: case 4: {
				MENUITEMINFO info = {.cbSize = sizeof(MENUITEMINFO), .fMask = MIIM_STATE};
				GetMenuItemInfo(menu, index, 0, &info);
				int newVal = !(info.fState & MFS_CHECKED);
				if (index == 3)
					data->type = DEST_LAST + 1;
				gSettings.flags = (gSettings.flags & ~(1<<data->type)) | newVal<<data->type;
			}	break;
			case 6: {
				User *u = FindUser(data->name);
				if (u)
					u->ignore = 1;
			}	// FALLTHROUGH:
			case 5:
				SendMessage(data->type == DEST_BATTLE ? GetParent(window) : window, WM_CLOSE, 0, 0);
				break;
			}
			
		} else if (note->idFrom == DLG_LOG && note->code == EN_LINK) {
			ENLINK *s = (void *)lParam;
			if (s->msg != WM_LBUTTONUP)
				break;
			SendMessage(note->hwndFrom, EM_EXSETSEL, 0, (LPARAM)&s->chrg);
			wchar_t url[256];
			SendMessage(note->hwndFrom, EM_GETTEXTEX,
					(WPARAM)&(GETTEXTEX){.cb = sizeof(url), .flags = GT_SELECTION, .codepage = 1200},
					(LPARAM)url);
			ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
			return 0;
		}
	}	break;
	}
	return DefWindowProc(window, msg, wParam, lParam);
}

static const COLORREF chatColors[CHAT_LAST+1] = {
	[CHAT_EX] = RGB(225,152,163),
	[CHAT_INGAME] = RGB(90,122,150),
	[CHAT_SELF] = RGB(50,170,230),
	[CHAT_SELFEX] = RGB(50,170,230),
	[CHAT_SYSTEM] = RGB(80,150,80),
	[CHAT_TOPIC] = RGB(150,130,80),

	[CHAT_SERVERIN] = RGB(153,190,215),
	[CHAT_SERVEROUT] = RGB(215,190,153),
};

#define ALIAS(c)\
	RGB(128 + GetRValue(c) / 2, 128 + GetGValue(c) / 2, 128 + GetBValue(c) / 2)
#define COLOR_TIMESTAMP RGB(200,200,200)

static void putText(HWND window, const char *text, COLORREF color, DWORD effects)
{
	SendMessage(window, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&(CHARFORMAT){
		.cbSize = sizeof(CHARFORMAT),
		.dwMask = CFM_BOLD | CFM_COLOR | CFM_ITALIC | CFM_STRIKEOUT | CFM_UNDERLINE,
		.dwEffects = effects,
		.crTextColor = color,
	});
	SendMessage(window, EM_SETTEXTEX, (WPARAM)&(SETTEXTEX){.flags = ST_SELECTION, .codepage = 65001}, (LPARAM)text);
}

void Chat_Said(HWND window, const char *username, ChatType type, const char *text)
{
	window = GetDlgItem(window, DLG_LOG);
	SendMessage(window, EM_EXSETSEL, 0, (LPARAM)&(CHARRANGE){INFINITE, INFINITE});
	
	char buff[128];
	if ((type != CHAT_TOPIC) && gSettings.flags & SETTING_TIMESTAMP) {
		char *s = buff;
		*(s++) = '[';
		s += GetTimeFormatA(0, 0, NULL, NULL, s, sizeof(buff) - 2) - 1;
		*(s++) = ']';
		*(s++) = ' ';
		*s = '\0';
		putText(window, buff, COLOR_TIMESTAMP, 0);
	}
	
	COLORREF color = chatColors[type];
	if (type == CHAT_SERVERIN || type == CHAT_SERVEROUT)
		putText(window, type == CHAT_SERVERIN ? "> " : "< ", color, 0);

	if (username) {
		putText(window, username, color, !(type & CHAT_SYSTEM) * CFE_BOLD);
		const User *u = FindUser(username);
		const char *alias = u ? u->alias : "logged out";
		if (strcmp(alias, username)) {
			sprintf(buff, " (%s)", alias);
			putText(window, buff, ALIAS(color), 0);
		}
		putText(window, type & (CHAT_EX | CHAT_SYSTEM) ? " " : ": ", color, !(type & CHAT_SYSTEM) * CFE_BOLD);
	}
	
	putText(window, text, color, 0);
	putText(window, "\n", color, 0);

	if (GetTickCount() - GetWindowLongPtr(window, GWLP_USERDATA) > 5000) {
		SendMessage(window, WM_VSCROLL, SB_BOTTOM, 0);
		SetWindowLongPtr(window, GWLP_USERDATA, 0);
	}
}

HWND GetChannelChat(const char *name)
{
	for (int i=0; i<lengthof(channelWindows); ++i) {
		if (!channelWindows[i]) {
			chatWindowData *data = malloc(sizeof(chatWindowData));
			*data = (chatWindowData){strdup(name), DEST_CHANNEL};
			channelWindows[i] = CreateWindow(WC_CHATBOX, NULL, WS_CHILD,
			0, 0, 0, 0,
			gChatWindow, (HMENU)DEST_CHANNEL, NULL, (void *)data);
			return channelWindows[i];
		}
		chatWindowData *data = (void *)GetWindowLongPtr(channelWindows[i], GWLP_USERDATA);
		if (!strcmp(name, data->name))
			return channelWindows[i];
	}
	return NULL;
}

HWND GetPrivateChat(User *u)
{
	if (!u->chatWindow) {
		chatWindowData *data = malloc(sizeof(chatWindowData));
		*data = (chatWindowData){u->name, DEST_PRIVATE};
		u->chatWindow = CreateWindow(WC_CHATBOX, NULL, WS_CHILD,
			0, 0, 400, 400,
			gChatWindow, (HMENU)DEST_PRIVATE, NULL, (void *)data);
	}
	return u->chatWindow;
}

// HWND GetServerChat(void)
// {
	// if (!_gServerChatWindow)
		// _gServerChatWindow = CreateWindow(WC_CHATBOX, NULL, WS_CHILD, 0, 0, 0, 0, gMainWindow, (HMENU)DEST_SERVER, NULL, (void *)"TAS Server");
	// return _gServerChatWindow;
// }

void SaveLastChatWindows(void)
{
	char autojoinChannels[10000];
	autojoinChannels[0] = 0;
	size_t len = 0;
	for (int i=0; i<lengthof(channelWindows); ++i) {
		SendDlgItemMessage(channelWindows[i], DLG_LIST, LVM_DELETEALLITEMS, 0, 0);
		if (GetTabIndex(channelWindows[i]) >= 0) {
			chatWindowData *data = (void *)GetWindowLongPtr(channelWindows[i], GWLP_USERDATA);
			len += sprintf(&autojoinChannels[len], "%s%s", len ? ";" : "", data->name);
		}
	}
	free(gSettings.autojoin);
	gSettings.autojoin = strdup(autojoinChannels);
}

void Chat_Init(void)
{
	RegisterClass((&(WNDCLASS){
		.lpszClassName = WC_CHATBOX,
		.lpfnWndProc   = chatBoxProc,
		.hCursor       = LoadCursor(NULL, (void *)(IDC_ARROW)),
		.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
	}));

	chatWindowData *data = malloc(sizeof(chatWindowData));
	*data = (chatWindowData){"TAS Server", DEST_SERVER};
	_gServerChatWindow = CreateWindow(WC_CHATBOX, NULL, WS_CHILD, 0, 0, 0, 0, gChatWindow, NULL, NULL, (void *)data);
	ChatWindow_SetActiveTab(_gServerChatWindow);
}




